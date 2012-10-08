#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_core.h"
#include "fxmy_odbc.h"
#include "fxmy_query.h"
#include "fxmy_string.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#pragma warning(push, 0)
#include <sql.h>
#include <sqlext.h>
#include <Windows.h>
#pragma warning(pop)

fxmy_char *
fxmy_create_query_string(uint8_t *query_bytes, size_t query_num_bytes)
{
    LPVOID memory;
    size_t wide_string_length;
    size_t alloc_size;
    const char *query_string = (const char *)query_bytes;
    fxmy_char *string;

    wide_string_length = fxmy_fstrlenfromchar(query_string, query_num_bytes);
    alloc_size = (wide_string_length + 1) * sizeof(fxmy_char);
    memory = VirtualAlloc(NULL, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    string = fxmy_fstrfromchar(memory, query_string, query_num_bytes);
    string[wide_string_length] = 0;

    return string;
}

void
fxmy_destroy_query_string(fxmy_char *query)
{
    VirtualFree(query, 0, MEM_RELEASE);
}

#else

static fxmy_char *
fxmy_create_query_string(uint8_t *query_bytes, size_t query_num_bytes)
{
    return (fxmy_char *)query_bytes;
}

static void
fxmy_destroy_query_string(fxmy_char *query)
{
}

#endif

static int
fxmy_describe(struct fxmy_connection_t *conn, const char *query)
{
    SQLHANDLE query_handle;
    struct fxmy_odbc_t *odbc;
    const struct fxmy_status_t *status;
    const char *describe_end;
    const char *table_begin;
    const char *table_end;
    fxmy_char *wide_buf;
    fxmy_char columns[] = C("sp_columns ");
    
    odbc = conn->odbc;

    fxmy_next_token(&describe_end, query);
    table_begin = fxmy_next_token(&table_end, describe_end);

    wide_buf = calloc(1024, sizeof(fxmy_char));

    fxmy_fstrncpy(wide_buf, columns, 1024);
    fxmy_fstrncatfromchar(wide_buf, 1024, table_begin, table_end - table_begin);

    VERIFY_ODBC(SQLAllocHandle(
            SQL_HANDLE_STMT,
            odbc->database_connection_handle,
            &query_handle),
        SQL_HANDLE_DBC,
        odbc->database_connection_handle);

    status = fxmy_verify_and_log_odbc(SQLExecDirect(
            query_handle,
            wide_buf,
            SQL_NTS),
        SQL_HANDLE_STMT,
        query_handle);

    if (FXMY_FAILED(status))
    {
        conn->status = status;
        goto column_query_failed;
    }

    for (;;)
    {
        status = fxmy_verify_and_log_odbc(SQLFetch(query_handle), SQL_HANDLE_STMT, query_handle);

        if (FXMY_EMPTY_SET(status))
        {
            status = fxmy_get_status(FXMY_ERROR_UNKNOWN_OBJECT);
            goto cleanup;
        }

        __debugbreak();
    }

cleanup:
    VERIFY_ODBC(SQLFreeHandle(SQL_HANDLE_STMT, query_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);

column_query_failed:
    free(wide_buf);

    return 0;
}

/*
 * MySQL selects a certain number of rows by using a a "SELECT ... LIMIT x"
 * query whereas MSSQL uses a "SELECT TOP x ..." style query. This function,
 * fxmy_rearrange_limit, converts the former to the latter.
 */
static int
fxmy_rearrange_limit(struct fxmy_connection_t *conn, fxmy_char *query)
{
    const fxmy_char *limit_begin;
    const fxmy_char *limit_end;
    const fxmy_char *num_begin;
    const fxmy_char *num_end;
    const fxmy_char *select_begin;
    const fxmy_char top[] = C("TOP  ");
    fxmy_char *num_storage;
    fxmy_char *next_token_after_select;
    int shift_width;
    int number_string_length;
    size_t bytes_to_shift;

    select_begin = NULL;
    next_token_after_select = NULL;
    limit_begin = query;

    /*
     * This loop iterates through the query string one token at a time in
     * order to identify the LIMIT clause.
     */
    for (;;)
    {
        limit_begin = fxmy_fnext_token(&limit_end, limit_begin);

        /*
         * The "TOP x" bit is copied to the front of the query after the 
         * SELECT but we first need to identify where the first token
         * after the select begins.
         */
        if (!select_begin && fxmy_fstrnicmp(limit_begin, C("SELECT"), 6) == 0)
        {
            select_begin = limit_begin;
        } 
        else if (!next_token_after_select && select_begin) 
        {
            /*
             * This code purposely casts away const as we need to write to
             * this address further down the program. The limit_begin
             * variable is otherwise const to prevent unintended writes
             * from happening through it.
             */
            next_token_after_select = (fxmy_char *)limit_begin;
        }

        /*
         * If we find the LIMIT clause, break out of the loop, setting the 
         * beginning of our number string to be the end of the limit clause.
         * The exact bounds of the number string are determined below.
         */
        if (fxmy_fstrnicmp(limit_begin, C("LIMIT"), 5) == 0)
        {
            num_begin = limit_end;
            break;
        }

        limit_begin = limit_end;
    }

    /*
     * If we can't find a limit_begin, bail.
     */
    if (!limit_begin)
    {
        conn->status = fxmy_get_status(FXMY_ERROR_DEFAULT);
        return 1;
    }

    /*
     * Determine the bounds and length of the number string.
     */
    num_begin = fxmy_fnext_token(&num_end, num_begin);
    number_string_length = num_end - num_begin;

    /*
     * Allocate a temporary buffer that we copy the number string into.
     * This is allocated on the heap, as are all temporary buffers in
     * the program, so that a rogue copy operation doesn't end up 
     * trampling all over our stack. This buffer is allocated to include
     * space for the string itself, a space character, and a null
     * terminator.
     */
    num_storage = calloc(number_string_length + 2, sizeof(fxmy_char));
    memmove(num_storage, num_begin, number_string_length);

    /*
     * The number string has a space appended as we need to separate
     * that string from the next token in the stream.
     */
    num_storage[number_string_length] = C(' ');

    shift_width = num_end - limit_begin;
    bytes_to_shift = (limit_begin - next_token_after_select) * sizeof(fxmy_char);

    /*
     * Shift the string down to stomp over the LIMIT x clause.
     */
    memmove(next_token_after_select + shift_width, next_token_after_select, bytes_to_shift);

    /*
     * Copy in the "TOP  " string into our buffer.
     */
    memmove(next_token_after_select, top, (FXMY_ARRAY_COUNT(top) - 1) * sizeof(fxmy_char));

    /*
     * Copy in our number string last.
     */
    memmove(next_token_after_select + FXMY_ARRAY_COUNT(top) - 1, num_storage, (number_string_length + 1) * sizeof(fxmy_char));

    /*
     * At this point our num_storage buffer is no longer needed and can be
     * returned to the general heap.
     */
    free(num_storage);
    return 0;
}

int
fxmy_handle_query(struct fxmy_connection_t *conn, uint8_t *query_string, size_t query_num_bytes)
{
    const char *query;
    struct fxmy_odbc_t *odbc;

    /* 
     * Query strings are guaranteed to be null terminated by fxmy_recv.
     */

    query = (const char *)query_string;
    odbc = conn->odbc;

    UNUSED(conn);
    UNUSED(query_num_bytes);

    if (fxmy_stristr(query, "SET NAMES") != NULL)
    {
        VERIFY(fxmy_stristr(query, "utf8") != NULL);
        return 0;
    }
    else if (fxmy_stristr(query, "DESCRIBE") != NULL)
    {
        return fxmy_describe(conn, query);
    }
    else if (fxmy_stristr(query, "SHOW TABLES") != NULL)
    {
        __debugbreak();
    }
    else
    {
        SQLHANDLE query_handle;
        fxmy_char *wide_query;
        const struct fxmy_status_t *status;

        wide_query = calloc(query_num_bytes + 1, sizeof(fxmy_char));
        fxmy_fstrfromchar(wide_query, query, query_num_bytes);
        wide_query[query_num_bytes] = 0;

        /*
         * SELECT xxx LIMIT y must be transformed into SELECT TOP y xxx
         */

        if (fxmy_fstristr(wide_query, C("LIMIT ")))
            if (fxmy_rearrange_limit(conn, wide_query))
                return 0;

        VERIFY_ODBC(SQLAllocHandle(SQL_HANDLE_STMT, odbc->database_connection_handle, &query_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);
        status = fxmy_verify_and_log_odbc(SQLExecDirect(query_handle, wide_query, SQL_NTS), SQL_HANDLE_STMT, query_handle);

        if (FXMY_FAILED(status))
        {
            conn->status = status;
        }
        else
        {
            __debugbreak();
        }

        VERIFY_ODBC(SQLFreeHandle(SQL_HANDLE_STMT, query_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);
        free(wide_query);
    }

    /*
    query = fxmy_create_query_string(query_bytes, query_num_bytes);

    __debugbreak();

    fxmy_destroy_query_string(query);

    */
    return 0;
}
