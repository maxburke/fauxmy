#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_core.h"
#include "fxmy_error.h"

struct fxmy_error_table_t
{
    int mssql_error_code;
    int mysql_error_code;
    const char *mysql_sql_state;
};

/* 
 * I'm thinking that 1149/42000 (syntax error) may be a good fallback.
 */

#define FXMY_DEFAULT_ERROR 208

static const struct fxmy_status_t fxmy_error_table[] = {
    { 0, 0, 0, "", "OK" },
    { 102, 0xFF, 1149, "42000", "ERROR" },
    { 109, 0xFF, 1136, "21S01", "ERROR" },
    { 110, 0xFF, 1136, "21S01", "ERROR" },
    { 120, 0xFF, 1136, "21S01", "ERROR" },
    { 121, 0xFF, 1136, "21S01", "ERROR" },
    { 132, 0xFF, 1308, "42000", "ERROR" },
    { 134, 0xFF, 1331, "42000", "ERROR" },
    { 137, 0xFF, 1327, "42000", "ERROR" },
    { 139, 0xFF, 1067, "42000", "ERROR" },  /* VERIFY */
    { 144, 0xFF, 1056, "42000", "ERROR" },  /* VERIFY */
    /* 145 possibly requires special treatment */
    { 148, 0xFF, 1149, "42000", "ERROR" },
    { 156, 0xFF, 1149, "42000", "ERROR" },
    { 178, 0xFF, 1313, "42000", "ERROR" },
    { 183, 0xFF, 1425, "42000", "ERROR" },
    { 189, 0xFF, 1318, "42000", "ERROR" },
    { 193, 0xFF, 1071, "42000", "ERROR" },  /* VERIFY */
    { 201, 0xFF, 1107, "HY000", "ERROR" }, 
    { 207, 0xFF, 1054, "42S22", "ERROR" },
    { 208, 0xFF, 1146, "42S02", "ERROR" },
    { 214, 0xFF, 1108, "HY000", "ERROR" },
    { 216, 0xFF, 1583, "42000", "ERROR" },  /* VERIFY */ 
    { 5701, 0, 0, 0, "" },                  /* Changed database context. */
    { 5703, 0, 0, 0, "" },                  /* Changed language setting. */
};

const struct fxmy_status_t *
fxmy_get_status(int native_error)
{
    const size_t num_error_table_entries = FXMY_ARRAY_COUNT(fxmy_error_table);
    size_t i;

    for (i = 0; i < num_error_table_entries; ++i)
    {
        if (fxmy_error_table[i].native_error_code == native_error)
            return &fxmy_error_table[i];
    }

    __debugbreak();

    return fxmy_get_status(FXMY_DEFAULT_ERROR);
}
