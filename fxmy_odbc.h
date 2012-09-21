#ifndef FXMY_ODBC_H
#define FXMY_ODBC_H

#pragma once

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#pragma warning(push, 0)
#include <WTypes.h>
#include <sqltypes.h>
#pragma warning(pop)
#endif

struct fxmy_odbc_t
{
    SQLHENV environment_handle;
    SQLHDBC database_connection_handle;
    int connected;
};

int
fxmy_verify_and_log_odbc(SQLRETURN return_code, SQLSMALLINT handle_type, SQLHANDLE handle);

#define VERIFY_ODBC(return_value, handle_type, handle)                  \
    if (fxmy_verify_and_log_odbc(return_value, handle_type, handle))    \
    {                                                                   \
        __debugbreak();                                                 \
    }                                                                   \
    else (void)0

#endif