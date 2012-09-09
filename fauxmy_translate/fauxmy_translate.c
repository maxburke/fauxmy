#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define FXMY_EXPORT __declspec(dllexport)
#pragma warning(push, 0)
#include <WTypes.h>
#include <sql.h>
#include <sqlext.h>
#pragma warning(pop)
#endif

#define UNUSED(x) (void)x

/*
 * This module handles the character set conversion between the driver
 * and the data source. Currently it's unimplemented because MSSQL does
 * not handle this functionality.
 */

FXMY_EXPORT
BOOL WINAPI SQLDataSourceToDriver(
    UDWORD option,
    SWORD sql_type,
    PTR value_in,
    SDWORD value_in_length,
    PTR value_out,
    SDWORD value_out_max,
    SDWORD *value_out_size,
    UCHAR *error_msg,
    SDWORD error_msg_max,
    SDWORD *error_msg_bytes)
{
    UNUSED(option);
    UNUSED(sql_type);
    UNUSED(value_in);
    UNUSED(value_in_length);
    UNUSED(value_out);
    UNUSED(value_out_max);
    UNUSED(value_out_size);
    UNUSED(error_msg);
    UNUSED(error_msg_max);
    UNUSED(error_msg_bytes);

    __debugbreak();

    return TRUE;
}

FXMY_EXPORT
BOOL WINAPI SQLDriverToDataSource(
    UDWORD option,
    SWORD sql_type,
    PTR value_in,
    SDWORD value_in_length,
    PTR value_out,
    SDWORD value_out_max,
    SDWORD *value_out_size,
    UCHAR *error_msg,
    SDWORD error_msg_max,
    SDWORD *error_msg_bytes)
{
    UNUSED(option);
    UNUSED(sql_type);
    UNUSED(value_in);
    UNUSED(value_in_length);
    UNUSED(value_out);
    UNUSED(value_out_max);
    UNUSED(value_out_size);
    UNUSED(error_msg);
    UNUSED(error_msg_max);
    UNUSED(error_msg_bytes);

    __debugbreak();

    return TRUE;
}

