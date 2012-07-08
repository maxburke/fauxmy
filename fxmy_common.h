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
    #include <stdint.h>
    #pragma warning(pop)
    #pragma warning(disable:4514)

    #define FXMY_PERROR fxmy_perror

    void 
    fxmy_perror(const char *string);
    struct fxmy_connection_t;

    struct fxmy_connection_t
    {
        OVERLAPPED overlapped;
        SOCKET socket;
        struct fxmy_xfer_buffer_t xfer_buffer;
        uint32_t client_flags;
        uint32_t max_packet_size;
        uint32_t charset;
        char *database;
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

#define CLIENT_LONG_PASSWORD 1
#define CLIENT_FOUND_ROWS 2
#define CLIENT_LONG_FLAG 4
#define CLIENT_CONNECT_WITH_DB 8
#define CLIENT_NO_SCHEMA 16
#define CLIENT_COMPRESS 32
#define CLIENT_ODBC 64
#define CLIENT_LOCAL_FILES 128
#define CLIENT_IGNORE_SPACE 256
#define CLIENT_PROTOCOL_41 512
#define CLIENT_INTERACTIVE 1024
#define CLIENT_SSL 2048
#define CLIENT_IGNORE_SIGPIPE 4096
#define CLIENT_TRANSACTIONS 8192
#define CLIENT_RESERVED 16384
#define CLIENT_SECURE_CONNECTION 32768
#define CLIENT_MULTI_STATEMENTS 65536
#define CLIENT_MULTI_RESULTS 131072

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