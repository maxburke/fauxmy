#ifndef FXMY_MAIN_H
#define FXMY_MAIN_H

struct fxmy_connection_t;

void
fxmy_worker(const struct fxmy_connection_context_t *context);

struct fxmy_connection_t *
fxmy_conn_create(void);

void
fxmy_conn_dispose(struct fxmy_connection_t *conn);

#endif