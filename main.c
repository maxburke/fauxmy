/* 
   fauxmy (foamie)
   Fake MySQL server.
*/

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdint.h>

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#define FXMY_DEFAULT_PORT 3306
#define FXMY_DEFAULT_LISTEN_BACKLOG 50

#define VERIFY_impl(x, line) if (!(x)) { perror(__FILE__ "(" line "): " #x); abort(); } else (void)0
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define VERIFY(x) VERIFY_impl(x, STRINGIZE(__LINE__)) 

#define UNUSED(x) (void)x

/*
    Conventions:
    * Functions return 0 if they have work still to do, 1 if they are done. For example,
      if fxmy_send() returns 0, the transfer has not yet completed. If it returns 1, it has.
*/

struct fxmy_xfer_buffer_t
{
    void *memory;
    size_t size;
    size_t cursor;
};

enum fxmy_connection_state_t
{
    STATE_SEND_HANDSHAKE,
    STATE_RECV_AUTH_PACKET,
    STATE_SEND_ACK,
    STATE_RECV_COMMAND,
    STATE_SEND_RESPONSE,
    STATE_NUM_STATES
};

struct fxmy_connection_t
{
    enum fxmy_connection_state_t state;
    struct fxmy_xfer_buffer_t xfer_buffer;
    int socket_fd;
};

typedef int (*fxmy_connection_state_handler)(struct fxmy_connection_t *conn);

static int
fxmy_send_handshake(struct fxmy_connection_t *conn);

static int
fxmy_recv_auth_packet(struct fxmy_connection_t *conn);

static int
fxmy_send_ack(struct fxmy_connection_t *conn);

static int
fxmy_recv_command(struct fxmy_connection_t *conn);

static int
fxmy_send_response(struct fxmy_connection_t *conn);

fxmy_connection_state_handler handlers[STATE_NUM_STATES] = 
{
    fxmy_send_handshake,
    fxmy_recv_auth_packet,
    fxmy_send_ack,
    fxmy_recv_command,
    fxmy_send_response
};

static void
fxmy_reset_xfer_buffer(struct fxmy_xfer_buffer_t *buffer)
{
    free(buffer->memory);

    buffer->memory = NULL;
    buffer->size = 0;
    buffer->cursor = 0;
}

static int
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
static void
fxmy_begin_send(struct fxmy_connection_t *conn, const void *data, size_t size)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    const unsigned char *src = data;
    unsigned char *dest;
    size_t i;
    size_t alloc_size = size;
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
        dest[3] = (unsigned char)i;

        memcpy(dest + 4, src, packet_size);
        dest += packet_size + 4;
        src += packet_size;
    }
}

static int
fxmy_send(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    ssize_t rv;
    void *start;
    size_t size;

    VERIFY(fxmy_xfer_in_progress(buffer));
    VERIFY(buffer->size > 0);
    VERIFY(buffer->size > buffer->cursor);

    start = (char *)buffer->memory + buffer->cursor;
    size = buffer->size - buffer->cursor;
    rv = write(conn->socket_fd, start, size);

    /* 
       Any result other than EAGAIN/EWOULDBLOCK, or a valid transfer amount,
       is considered a failure. EAGAIN/EWOULDBLOCK are only allowed because
       the socket is set as nonblocking when it is first created.
    */
    VERIFY(rv >= 0 || rv == EAGAIN || rv == EWOULDBLOCK);

    if (rv >= 0)
        buffer->cursor += (size_t)rv;

    if (buffer->size == buffer->cursor)
    {
        fxmy_reset_xfer_buffer(buffer);
        return 1;
    }

    return 0;
}

/*
   receive algorithm:
   * This one is a bit more difficult. The algorithm has to get the header 
     first and see how much it needs to receive.
*/

static void
fxmy_begin_recv(int fd, struct fxmy_xfer_buffer_t *buffer)
{
    unsigned char packet_header[4];
    ssize_t rv;
    /* int sequence_number; */
    size_t packet_size;

    rv = read(fd, packet_header, sizeof packet_header);

    VERIFY(rv == 4);

    /* 
        TODO: Handle receiving data larger than 16mb. For now this just asserts
        if the packet size is too large.
     */

    packet_size = (size_t)(packet_header[0])
        | ((size_t)(packet_header[1]) << 8)
        | ((size_t)(packet_header[2]) << 16);
    /* sequence_number = (int)packet_header[3]; */

    buffer->memory = malloc(packet_size);
    buffer->size += packet_size;

printf("RECEIVING PACKET: size: %lu\n", packet_size);
}

static int
fxmy_recv(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    ssize_t rv;
    char *read_ptr;
    size_t read_size;

    if (!fxmy_xfer_in_progress(buffer))
        fxmy_begin_recv(conn->socket_fd, buffer);

    read_ptr = (char *)buffer->memory + buffer->cursor;
    read_size = buffer->size - buffer->cursor;
    rv = read(conn->socket_fd, read_ptr, read_size);

    VERIFY(rv >= 0 || rv == EAGAIN || rv == EWOULDBLOCK);
    buffer->cursor += rv;

    return buffer->size == buffer->cursor;
}

static int
fxmy_socket_open(int port)
{
    int socket_fd;
    struct sockaddr_in addr;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    VERIFY(socket_fd != -1);

    memset(&addr, 0, sizeof addr);
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    VERIFY(bind(socket_fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    VERIFY(listen(socket_fd, FXMY_DEFAULT_LISTEN_BACKLOG) == 0);

    return socket_fd;
}

static void
fxmy_accept_connection(int epoll_fd, int socket_fd)
{
    int new_connection_fd;
    struct epoll_event event;
    struct sockaddr_in connection;
    socklen_t size = sizeof(connection);
    
    memset(&event, 0, sizeof event);
    new_connection_fd = accept(socket_fd, (struct sockaddr *)&connection, &size);
    VERIFY(new_connection_fd != -1);
    fcntl(new_connection_fd, F_SETFD, O_NONBLOCK);
    event.events = EPOLLIN | EPOLLOUT;
    event.data.fd = new_connection_fd;
    VERIFY(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_connection_fd, &event) != -1);
}

static void
fxmy_get_handshake_packet(void **ptr, size_t *size)
{
    static char handshake_packet[] = {
        0xa,                            /* protocol version */
        '4', '.', '1', '.', '1', 0,     /* server version (null terminated) */
        1, 0, 0, 0,                     /* thread id */
        0, 0, 0, 0, 0, 0, 0, 0,         /* scramble buf */
        0,                              /* filler, always 0 */
        0, 0x2,                         /* server capabilities. the 2 (512, in
                                           little endian) means that this
                                           server supports the 4.1 protocol
                                           instead of the 4.0 protocol. */
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

static int
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

static int
fxmy_recv_auth_packet(struct fxmy_connection_t *conn)
{
    if (fxmy_recv(conn))
    {
        FILE *fp = fopen("dump", "w");
        fwrite(conn->xfer_buffer.memory, conn->xfer_buffer.size, 1, fp);
        fclose(fp);
        return 1;
    }

    return 0;
}

static int
fxmy_send_ack(struct fxmy_connection_t *conn)
{
    UNUSED(conn);
    return 1;
}

static int
fxmy_recv_command(struct fxmy_connection_t *conn)
{
    UNUSED(conn);
    return 1;
}

static int
fxmy_send_response(struct fxmy_connection_t *conn)
{
    UNUSED(conn);
    return 1;
}

static int
fxmy_handle_connection(int socket_fd)
{
    #define FXMY_MAX_CONNECTIONS 128
    static struct fxmy_connection_t connections[FXMY_MAX_CONNECTIONS];
    static int active_connections[FXMY_MAX_CONNECTIONS];
    static int num_connections;

    int i;
    enum fxmy_connection_state_t state;

    for (i = 0; i < num_connections; ++i)
    {
        if (active_connections[i] == socket_fd)
            break;
    }

    VERIFY(i < FXMY_MAX_CONNECTIONS);
    active_connections[i] = socket_fd;
    connections[i].socket_fd = socket_fd;

    state = connections[i].state;
    if (handlers[state](&connections[i]))
        ++connections[i].state;

    return 0;
}

int main(void) 
{
    #define MAX_NUM_EVENTS 128
    int epoll_fd;
    int db_socket_fd;
    struct epoll_event event;
    struct epoll_event events[MAX_NUM_EVENTS];
   
    epoll_fd = epoll_create1(0);
    VERIFY(epoll_fd >= 0);
    db_socket_fd = fxmy_socket_open(FXMY_DEFAULT_PORT);

    memset(&event, 0, sizeof event);
    memset(events, 0, sizeof events);

    event.events = EPOLLIN;
    event.data.fd = db_socket_fd;
    VERIFY(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, db_socket_fd, &event) == 0);

    for (;;)
    {
        int num_ready_fds;
        int i;

        num_ready_fds = epoll_wait(epoll_fd, events, MAX_NUM_EVENTS, -1);
        VERIFY(num_ready_fds != -1);

        for (i = 0; i < num_ready_fds; ++i)
        {
            int current_fd = events[i].data.fd;

            if (current_fd == db_socket_fd)
            {
printf("CONNECTION!\n");
                fxmy_accept_connection(epoll_fd, db_socket_fd);
            }
            else
            {
printf("HANDLING CONNECTION!\n");
                if (fxmy_handle_connection(current_fd))
                {
printf("CONNECTION DONE!\n");
                    VERIFY(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL) != -1);
                    close(current_fd);
                }
            }
        }
    }

    close(db_socket_fd);
    close(epoll_fd);

    return 0;
}

