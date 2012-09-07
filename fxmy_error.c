#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_error.h"

static const struct fxmy_status_t fxmy_status[] = 
{
    { 0,    0,      "",     "OK" },                         /* FXMY_STATUS_OK */
    { 0xFF, 1045,   "28000", "Bad password" },              /* FXMY_STATUS_BAD_PASSWORD */
    { 0xFF, 1049,   "42000", "Unknown database" },          /* FXMY_STATUS_UNKNOWN_DATABASE */
};

const struct fxmy_status_t *
fxmy_get_status(enum fxmy_status_code_t status_code)
{
    VERIFY(status_code < FXMY_LAST_STATUS_CODE);
    return &fxmy_status[status_code];
}
