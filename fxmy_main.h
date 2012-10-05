#ifndef FXMY_MAIN_H
#define FXMY_MAIN_H

#pragma once

struct fxmy_connection_t;

void
fxmy_worker(struct fxmy_connection_t *context);

struct fxmy_connection_t *
fxmy_conn_create(void);

void
fxmy_conn_dispose(struct fxmy_connection_t *conn);

#endif