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

static void
fxmy_rearrange_limit(fxmy_char *query)
{
    UNUSED(query);
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
            fxmy_rearrange_limit(wide_query);

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