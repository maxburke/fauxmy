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

#include "fxmy_common.h"

#define FXMY_DEFAULT_PORT 3306
#define FXMY_DEFAULT_LISTEN_BACKLOG 50

/*
    Conventions:
    * Functions return 0 if they have work still to do, 1 if they are done. For example,
      if fxmy_send() returns 0, the transfer has not yet completed. If it returns 1, it has.
*/

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

