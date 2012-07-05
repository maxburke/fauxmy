#ifndef FXMY_COMMON_H
#define FXMY_COMMON_H

struct fxmy_xfer_buffer_t
{
    void *memory;
    size_t size;
    size_t cursor;
};

#ifdef _MSC_VER
    #pragma warning(push, 0)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <WinSock2.h>
    #include <MSWSock.h>
    #pragma warning(pop)
    #pragma warning(disable:4514)

    #define FXMY_PERROR fxmy_perror

    void 
    fxmy_perror(const char *string);

    struct fxmy_connection_t;
    typedef int (*fxmy_completion_callback)(struct fxmy_connection_t *conn);

    struct fxmy_connection_t
    {
        OVERLAPPED overlapped;
        fxmy_completion_callback callback;
        SOCKET socket;
        DWORD bytes_transferred;
        struct fxmy_xfer_buffer_t xfer_buffer;
    };

#else
    #define FXMY_PERROR perror

    struct fxmy_connection_t
    {
        enum fxmy_connection_state_t state;
        struct fxmy_xfer_buffer_t xfer_buffer;
        int socket_fd;
    };
    
#endif

#define VERIFY_impl(x, line) if (!(x)) { FXMY_PERROR(__FILE__ "(" line "): " #x); abort(); } else (void)0
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define VERIFY(x) VERIFY_impl(x, STRINGIZE(__LINE__)) 

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#define UNUSED(x) (void)x

enum fxmy_connection_state_t
{
    STATE_SEND_HANDSHAKE,
    STATE_RECV_AUTH_PACKET,
    STATE_SEND_ACK,
    STATE_RECV_COMMAND,
    STATE_SEND_RESPONSE,
    STATE_NUM_STATES
};

void
fxmy_reset_xfer_buffer(struct fxmy_xfer_buffer_t *buffer);

int
fxmy_xfer_in_progress(struct fxmy_xfer_buffer_t *buffer);

void
fxmy_begin_send(struct fxmy_connection_t *conn, const void *data, size_t size);

int
fxmy_send_handshake(struct fxmy_connection_t *conn);

void
fxmy_get_handshake_packet(void **ptr, size_t *size);

int
fxmy_send(struct fxmy_connection_t *conn);

#endif