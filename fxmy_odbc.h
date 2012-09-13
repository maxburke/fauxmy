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

#endif