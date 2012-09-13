#ifndef FXMY_CONN_H
#define FXMY_CONN_H

#pragma once

#include <stddef.h>

struct fxmy_connection_t;

int
fxmy_send(struct fxmy_connection_t *conn, const void *data, size_t size);

int
fxmy_recv(struct fxmy_connection_t *conn);

#endif