#ifndef FXMY_QUERY_H
#define FXMY_QUERY_H

#pragma once

#include <stdint.h>

struct fxmy_connection_t;

int
fxmy_handle_query(struct fxmy_connection_t *conn, uint8_t *query_string, size_t query_string_length);
#endif