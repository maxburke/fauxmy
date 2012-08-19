#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_conn.h"

void
fxmy_reset_xfer_buffer(struct fxmy_xfer_buffer_t *buffer)
{
    free(buffer->memory);

    buffer->memory = NULL;
    buffer->size = 0;
    buffer->cursor = 0;
}

int
fxmy_xfer_in_progress(struct fxmy_xfer_buffer_t *buffer)
{
    return buffer->memory != NULL;
}

int
fxmy_send_handshake(struct fxmy_connection_t *conn)
{
    void *ptr;
    size_t size;

    fxmy_get_handshake_packet(&ptr, &size);
    if (fxmy_send(conn, ptr, size))
        return 1;

    return 0;
}

void
fxmy_get_handshake_packet(void **ptr, size_t *size)
{
    static char handshake_packet[] = {
        0xa,                            /* protocol version */
        '4', '.', '1', '.', '1', 0,     /* server version (null terminated) */
        1, 0, 0, 0,                     /* thread id */
        0, 0, 0, 0, 0, 0, 0, 0,         /* scramble buf */
        0,                              /* filler, always 0 */
        0, 0x82,                        /* server capabilities. the 2 (512, in
                                           little endian) means that this
                                           server supports the 4.1 protocol
                                           instead of the 4.0 protocol. */
                                        /* The 0x80 indicates that we support 4.1
                                           protocol authentication, enforcing
                                           that the password is sent to us as
                                           a 20-byte re-jiggered SHA1. */
        0,                              /* server language */
        0, 0,                           /* server status */
        0, 0,                           /* server capabilities, upper two bytes */
        8,                              /* length of the scramble */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* filler, always 0 */
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,               /* plugin data, at least 12 bytes. */
        0                               /* second part of scramble */
    };

    VERIFY(ptr != NULL);
    VERIFY(size != NULL);

    *ptr = handshake_packet;
    *size = sizeof handshake_packet;
}
