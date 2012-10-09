#ifndef FXMY_QUERY_H
#define FXMY_QUERY_H

#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#include <stdint.h>
#pragma warning(pop)
#else
#include <stdint.h>
#endif

#include "fxmy_string_type.h"

struct fxmy_connection_t;

int
fxmy_handle_query(struct fxmy_connection_t *conn, uint8_t *query_string, size_t query_string_length);

/*
 * Helper functions follow. These are exposed only for the purposes of unit testing.
 */

const char *
fxmy_consume_whitespace(const char *ptr);

const char *
fxmy_find_end_of_string(const char *start, char string_char);

const char *
fxmy_next_token(const char **end_ptr, const char *str);

int
fxmy_rearrange_limit(fxmy_char *query);

#endif