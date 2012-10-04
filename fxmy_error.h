#ifndef FXMY_ERROR_H
#define FXMY_ERROR_H

#pragma once

#include <stdint.h>

struct fxmy_status_t
{
    int native_error_code;
    uint8_t header;
    uint16_t status_code;
    const char *sql_state;
    const char *message;
};

const struct fxmy_status_t *
fxmy_get_status(int native_error);

enum fxmy_error_code_t
{
    /*
     * Negative error codes are internal to Fauxmy. Positive error codes
     * are what MSSQL returns.
     */
    FXMY_ERROR_NO_DATA = -100,
    FXMY_ERROR_UNKNOWN_OBJECT = 208,
    FXMY_ERROR_DEFAULT = 208
};

#define FXMY_SUCCEEDED(x) ((x)->header == 0)
#define FXMY_FAILED(x) ((x)->header != 0)
#define FXMY_EMPTY_SET(x) ((x)->native_error_code == FXMY_ERROR_NO_DATA)

#endif