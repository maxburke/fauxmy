#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_core.h"
#include "fxmy_mem.h"
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

static const struct fxmy_status_t *
fxmy_exec(SQLHANDLE *query_handle, struct fxmy_connection_t *conn, const fxmy_char *query)
{
    struct fxmy_odbc_t *odbc;
    const struct fxmy_status_t *status;

    odbc = conn->odbc;

    VERIFY_ODBC(SQLAllocHandle(
            SQL_HANDLE_STMT,
            odbc->database_connection_handle,
            query_handle),
        SQL_HANDLE_DBC,
        odbc->database_connection_handle);

    status = fxmy_verify_and_log_odbc(SQLExecDirect(
            *query_handle,
            (fxmy_char *)query,
            SQL_NTS),
        SQL_HANDLE_STMT,
        query_handle);

    if (FXMY_FAILED(status))
    {
        VERIFY_ODBC(SQLFreeHandle(SQL_HANDLE_STMT, *query_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);
        *query_handle = NULL;
    }

    return status;
}

static int
fxmy_describe(struct fxmy_connection_t *conn, const fxmy_char *query)
{
    SQLHANDLE query_handle;
    struct fxmy_odbc_t *odbc;
    const struct fxmy_status_t *status;
    const fxmy_char *describe_end;
    const fxmy_char *table_begin;
    const fxmy_char *table_end;
    fxmy_char *buffer;
    const fxmy_char columns[] = C("sp_columns ");
    
    odbc = conn->odbc;

    fxmy_fnext_token(&describe_end, query);
    table_begin = fxmy_fnext_token(&table_end, describe_end);

    buffer = fxmy_calloc(1024, sizeof(fxmy_char));

    fxmy_fstrncpy(buffer, columns, 1024);
    fxmy_fstrncat(buffer, 1024, table_begin);

    conn->status = fxmy_exec(&query_handle, conn, buffer);

    if (FXMY_FAILED(conn->status))
        goto column_query_failed;

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
    fxmy_free(buffer);

    return 0;
}

static int
fxmy_show_tables(struct fxmy_connection_t *conn, const fxmy_char *query)
{
    SQLHANDLE query_handle;
    const struct fxmy_status_t *status;

    const fxmy_char *wildcard_begin;
    const fxmy_char *wildcard_end;
    const fxmy_char *token;
    const fxmy_char *format;
    fxmy_char *wildcard;
    fxmy_char *buffer;
    size_t num_elements;

    wildcard = NULL;
    wildcard_begin = NULL;
    wildcard_end = NULL;
    token = query;
    
    for (;;)
    {
        const fxmy_char *token_end;
        size_t token_length;

        token = fxmy_fnext_token(&token_end, token);

        if (token == NULL)
            break;

        token_length = token_end - token;

        if (fxmy_fstrnicmp(token, C("LIKE"), token_length) == 0)
        {
            wildcard_begin = fxmy_fnext_token(&wildcard_end, token_end);
            break;
        }

        token = token_end;
    }

    if (wildcard_begin != NULL)
    {
        size_t wildcard_size;

        VERIFY(wildcard_end >= wildcard_begin);
        wildcard_size = (wildcard_end + 1) - wildcard_begin;
        wildcard = fxmy_calloc(wildcard_size, sizeof(fxmy_char));
        
        memmove(wildcard, wildcard_begin, wildcard_size * sizeof(fxmy_char));
        format = C("sp_tables @table_owner='dbo', @table_name=%s");
    }
    else
    {
        format = C("sp_tables @table_owner='dbo'");
    }

    num_elements = fxmy_fsnprintf(NULL, 0, format, wildcard);
    buffer = fxmy_calloc(num_elements + 1, sizeof(fxmy_char));
    fxmy_fsnprintf(buffer, num_elements, format, wildcard);

    status = fxmy_exec(&query_handle, conn, buffer);
    
    if (query_handle != NULL)
    {
        status = fxmy_verify_and_log_odbc(SQLFetch(query_handle), SQL_HANDLE_STMT, query_handle);

        if (FXMY_EMPTY_SET(status))
        {
            conn->status = fxmy_get_status(FXMY_OK);
        }
    }

    fxmy_free(wildcard);
    fxmy_free(buffer);

    return 0;
}

/*
 * MySQL selects a certain number of rows by using a a "SELECT ... LIMIT x"
 * query whereas MSSQL uses a "SELECT TOP x ..." style query. This function,
 * fxmy_rearrange_limit, converts the former to the latter.
 */
int
fxmy_rearrange_limit(fxmy_char *query)
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
        size_t token_length;

        limit_begin = fxmy_fnext_token(&limit_end, limit_begin);
        token_length = limit_end - limit_begin;

        /*
         * The "TOP x" bit is copied to the front of the query after the 
         * SELECT but we first need to identify where the first token
         * after the select begins.
         */
        if (!select_begin && fxmy_fstrnicmp(limit_begin, C("SELECT"), token_length) == 0)
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
        if (fxmy_fstrnicmp(limit_begin, C("LIMIT"), token_length) == 0)
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
        return 1;

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
    num_storage = fxmy_calloc(number_string_length + 2, sizeof(fxmy_char));
    memmove(num_storage, num_begin, number_string_length * sizeof(fxmy_char));

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
    fxmy_free(num_storage);
    return 0;
}

int
fxmy_handle_query(struct fxmy_connection_t *conn, uint8_t *query_string, size_t query_num_bytes)
{
    const char *query;
    struct fxmy_odbc_t *odbc;
    fxmy_char *wide_query;

    /* 
     * Query strings are guaranteed to be null terminated by fxmy_recv.
     */

    query = (const char *)query_string;
    odbc = conn->odbc;

    wide_query = fxmy_calloc(query_num_bytes + 1, sizeof(fxmy_char));
    fxmy_fstrfromchar(wide_query, query, query_num_bytes);
    wide_query[query_num_bytes] = 0;

    if (fxmy_fstristr(wide_query, C("SET NAMES")) != NULL)
    {
        VERIFY(fxmy_fstristr(wide_query, C("utf8")) != NULL);
        return 0;
    }
    else if (fxmy_fstristr(wide_query, C("DESCRIBE")) != NULL)
    {
        return fxmy_describe(conn, wide_query);
    }
    else if (fxmy_fstristr(wide_query, C("SHOW TABLES")) != NULL)
    {
        return fxmy_show_tables(conn, wide_query);
    }
    else
    {
        SQLHANDLE query_handle;
        const struct fxmy_status_t *status;

        /*
         * SELECT xxx LIMIT y must be transformed into SELECT TOP y xxx
         */

        if (fxmy_fstristr(wide_query, C("LIMIT ")))
        {
            /*
             * fxmy_rearrange_limit returns an error condition if it can't
             * find the appropriate bounds of the LIMIT clause.
             */
            if (fxmy_rearrange_limit(wide_query))
            {
                conn->status = fxmy_get_status(FXMY_ERROR_DEFAULT);
                return 0;
            }
        }

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
    }

    /*
    query = fxmy_create_query_string(query_bytes, query_num_bytes);

    __debugbreak();

    fxmy_destroy_query_string(query);

    */

    fxmy_free(wide_query);
    return 0;
}
