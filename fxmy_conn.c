#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
    #pragma warning(push, 0)
    #include <WinSock2.h>
    #pragma warning(pop)
#endif

#include "fxmy_common.h"
#include "fxmy_conn.h"
#include "fxmy_core.h"

/* 
 * Before the data can be sent it has to be broken up into packets that are
 * no bigger than (1 << 24) - 1 bytes long. There can be at most 256 packets.
 * The packet header consists of a little endian 24-bit number expressing the
 * size of the data following the packet header plus an 8-bit packet ID number.
*/
static void
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

        memmove(dest + 4, src, packet_size);
        dest += packet_size + 4;
        src += packet_size;
    }
}

int
fxmy_send(struct fxmy_connection_t *conn, const void *data, size_t size)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    int bytes_written;

    fxmy_begin_send(conn, data, size);

    bytes_written = send(conn->socket, buffer->memory, buffer->size, 0);

    VERIFY((size_t)bytes_written == buffer->size);

    fxmy_reset_xfer_buffer(buffer);

    return 0;
}

static void
fxmy_begin_recv(struct fxmy_connection_t *conn)
{
    unsigned char packet_header[4];
    int rv;
    size_t packet_size;
    size_t packet_number;

    rv = recv(conn->socket, (char *)packet_header, sizeof packet_header, 0);
    VERIFY(rv == 4);

    packet_size = (size_t)(packet_header[0])
        | ((size_t)(packet_header[1]) << 8)
        | ((size_t)(packet_header[2]) << 16);
    packet_number = (size_t)packet_header[3];

    /* 
     * conn->packet_number holds the value of the next packet we operate on
     * so we validate that the packet we've received has the expected ID,
     * then increment it so that it is correct when we send the response. 
     */
    conn->packet_number = packet_number + 1;

    /*
     * The xfer_buffer is overallocated by one byte. Query strings are sent as
     * length-encoded strings which is a pain to handle in Fauxmy because they
     * are not null terminated, so by doing this we ensure that there will 
     * always be a null terminator and we can (relatively) safely use the C
     * string operators.
     */
    conn->xfer_buffer.memory = malloc(packet_size + 1);
    conn->xfer_buffer.size = packet_size;
}

int
fxmy_recv(struct fxmy_connection_t *conn)
{
    int bytes_read;
    char *buffer_ptr;
    size_t buffer_size;

    fxmy_begin_recv(conn);

    buffer_ptr = conn->xfer_buffer.memory;
    buffer_size = conn->xfer_buffer.size;
    bytes_read = recv(conn->socket, buffer_ptr, (int)buffer_size, 0);
    VERIFY((size_t)bytes_read == buffer_size);

    /*
     * The buffer is overallocated by one byte (see fxmy_begin_recv) for a null
     * terminator. This enforces that the zero terminator is always in place.
     */
    buffer_ptr[buffer_size] = 0;

    return 0;
}
