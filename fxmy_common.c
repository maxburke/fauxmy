#include <stdlib.h>

#include "fxmy_common.h"

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

/* 
   Before the data can be sent it has to be broken up into packets that are
   no bigger than (1 << 24) - 1 bytes long. There can be at most 256 packets.
   The packet header consists of a little endian 24-bit number expressing the
   size of the data following the packet header plus an 8-bit packet ID number.
*/
void
fxmy_begin_send(struct fxmy_connection_t *conn, const void *data, size_t size)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    const unsigned char *src = data;
    unsigned char *dest;
    size_t i;
    size_t alloc_size = size;
    size_t packet_number = conn->packet_number++;
    size_t num_packets = 0;

    if (fxmy_xfer_in_progress(buffer))
        return;

    for (i = 0; i < size; i += (1 << 24) - 1)
    {
        alloc_size += 4;
        ++num_packets;
    }

    VERIFY(num_packets <= 256);

    dest = malloc(alloc_size);
    buffer->memory = dest;
    buffer->size = alloc_size;
    buffer->cursor = 0;

    for (i = 0; i < num_packets; ++i)
    {
        size_t packet_size = MIN((1 << 24) - 1, size);
        size -= packet_size;
        dest[0] = (unsigned char)(packet_size & 0xFF);
        dest[1] = (unsigned char)((packet_size >> 8) & 0xFF);
        dest[2] = (unsigned char)((packet_size >> 16) & 0xFF);
        dest[3] = (unsigned char)packet_number;

        memcpy(dest + 4, src, packet_size);
        dest += packet_size + 4;
        src += packet_size;
    }
}

int
fxmy_send_handshake(struct fxmy_connection_t *conn)
{
    if (!fxmy_xfer_in_progress(&conn->xfer_buffer))
    {
        void *ptr;
        size_t size;

        fxmy_get_handshake_packet(&ptr, &size);
        fxmy_begin_send(conn, ptr, size);
    }

    if (fxmy_send(conn))
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
