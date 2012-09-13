#include "fxmy_common.h"
#include "fxmy_query.h"
#include "fxmy_odbc.h"
#include "fxmy_string.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#pragma warning(push, 0)
#include <sql.h>
#include <sqlext.h>
#include <Windows.h>
#pragma warning(pop)

static fxmy_char *
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

static void
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

int
fxmy_handle_query(struct fxmy_connection_t *conn, uint8_t *query_bytes, size_t query_num_bytes)
{
    size_t query_length;
    fxmy_char *query;

    /* 
     * Query strings are guaranteed to be null terminated by fxmy_recv.
     */

    if (stristr

    __debugbreak();

    /*
    query = fxmy_create_query_string(query_bytes, query_num_bytes);

    __debugbreak();

    fxmy_destroy_query_string(query);

    */
    return 0;
}