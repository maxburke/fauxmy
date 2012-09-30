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

    wide_string_length = fxmy_strlenfromchar(query_string, query_num_bytes);
    alloc_size = (wide_string_length + 1) * sizeof(fxmy_char);
    memory = VirtualAlloc(NULL, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    string = fxmy_strfromchar(memory, query_string, query_num_bytes);
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

enum fxmy_sql_token_t
{
    FXMY_TOKEN_INVALID,
    FXMY_TOKEN_SEMICOLON,
    FXMY_TOKEN_SET,
    FXMY_TOKEN_DESCRIBE
};

const char *
fxmy_stristr(const char *haystack, const char * const needle)
{
    const char *needle_ptr;
    const char *haystack_ptr;
    char h;
    char n;

    if (haystack == NULL || needle == NULL)
        return NULL;

    do
    {
        haystack_ptr = haystack;
        needle_ptr = needle;
        h = *haystack_ptr;
        n = *needle_ptr;

        while (tolower(h) == tolower(n))
        {
            /*
             * the n == 0 case here is implied as the inner body of the loop is
             * only entered if h == n.
             */
            if (h == 0)
                break;

            h = *++haystack_ptr;
            n = *++needle_ptr;
        }

        if (*needle_ptr == 0)
            return haystack;
    }
    while (*haystack++ != 0);

    return NULL;
}

int
fxmy_is_whitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

const char *
fxmy_consume_whitespace(const char *ptr)
{
    if (*ptr == 0)
        return ptr;

    while (fxmy_is_whitespace(*ptr))
        ++ptr;

    return ptr;
}

const char *
fxmy_find_end_of_string(const char *start, char string_char)
{
    char c;
    while ((c = *++start) != 0)
    {
        if (c == string_char)
            return start;
    }

    return NULL;
}

const char *
fxmy_next_token(const char **end_ptr, const char *str)
{
    const char *start;
    const char *end;
    char c;

    start = fxmy_consume_whitespace(str);
    end = NULL;

    if (start[0] == '-' && start[1] == '-')
    {
        while (*start && *start != '\n')
            ++start;
    }

    start = fxmy_consume_whitespace(str);
    end = start;
    c = *start;

    if (c == 0)
        goto NO_TOKEN;

    if (c == '\'' || c == '"')
    {
        end = fxmy_find_end_of_string(start, c);
        goto GOT_TOKEN;
    }

    for (;;)
    {
        c = *++end;
        if (fxmy_is_whitespace(c) || c == ';' || c == ',' || c == 0)
            goto GOT_TOKEN;
    }
    
GOT_TOKEN:
    *end_ptr = end;
    return start;

NO_TOKEN:
    *end_ptr = NULL;
    return NULL;
}

static int
fxmy_describe(const char *query)
{
    UNUSED(query);
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
        return fxmy_describe(query);
    }
    else
    {
        SQLHANDLE query_handle;
        fxmy_char *wide_query;
        const struct fxmy_status_t *status;

        wide_query = calloc(query_num_bytes + 1, sizeof(fxmy_char));
        fxmy_strfromchar(wide_query, query, query_num_bytes);

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