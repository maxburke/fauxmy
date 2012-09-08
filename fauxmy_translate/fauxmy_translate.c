#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define FXMY_EXPORT __declspec(dllexport)
#pragma warning(push, 0)
#include <WTypes.h>
#include <sql.h>
#include <sqlext.h>
#pragma warning(pop)
#endif

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
    return TRUE;
}