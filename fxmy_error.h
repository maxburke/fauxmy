#ifndef FXMY_ERROR_H
#define FXMY_ERROR_H

#pragma once

enum fxmy_status_code_t
{
    FXMY_STATUS_OK,
    FXMY_STATUS_BAD_PASSWORD,
    FXMY_STATUS_DATABASE_NOT_SELECTED,
    FXMY_STATUS_UNKNOWN_DATABASE,
    FXMY_LAST_STATUS_CODE
};

struct fxmy_status_t
{
    uint8_t header;
    uint16_t status_code;
    const char *sql_state;
    const char *message;
};

const struct fxmy_status_t *
fxmy_get_status(enum fxmy_status_code_t status_code);

#endif