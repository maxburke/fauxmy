#ifndef FXMY_COMMON_H
#define FXMY_COMMON_H

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(disable : 4820)
#endif

struct fxmy_xfer_buffer_t
{
    void *memory;
    size_t size;
    size_t cursor;
};


#ifdef _MSC_VER
    typedef unsigned int socket_t;

    #define FXMY_PERROR fxmy_perror

    void 
    fxmy_perror(const char *string);
#else
    typedef int socket_t;
#endif

struct fxmy_odbc_t;
struct fxmy_status_t;

#define FXMY_OK ((uint64_t)0)
#define FXMY_ERROR ((uint64_t)(int64_t)-1)

struct fxmy_connection_t
{
    socket_t socket;
    int done;
    struct fxmy_xfer_buffer_t xfer_buffer;
    uint32_t packet_number;
    uint32_t client_flags;
    uint32_t max_packet_size;
    uint32_t charset;
    char *database;
    uint16_t multi_statements_off;

    /*
     * Transient state. These values change from one query invocation to the
     * next.
     */
    uint64_t column_count_error_code;
    uint64_t affected_rows;
    uint64_t insert_id;
    const struct fxmy_status_t *status;

    struct fxmy_odbc_t *odbc;
    const char *connection_string;
};

#define VERIFY_impl(x, line) if (!(x)) { FXMY_PERROR(__FILE__ "(" line "): " #x); abort(); } else (void)0
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define VERIFY(x) VERIFY_impl(x, STRINGIZE(__LINE__))

#ifndef NDEBUG
#define ASSERT(x) VERIFY(x)
#else
#define ASSERT(x)
#endif

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

enum fxmy_client_flags_t
{
    CLIENT_LONG_PASSWORD        = 1 << 0,
    CLIENT_FOUND_ROWS           = 1 << 1,
    CLIENT_LONG_FLAG            = 1 << 2,
    CLIENT_CONNECT_WITH_DB      = 1 << 3,
    CLIENT_NO_SCHEMA            = 1 << 4,
    CLIENT_COMPRESS             = 1 << 5,
    CLIENT_ODBC                 = 1 << 6,
    CLIENT_LOCAL_FILES          = 1 << 7,
    CLIENT_IGNORE_SPACE         = 1 << 8,
    CLIENT_PROTOCOL_41          = 1 << 9,
    CLIENT_INTERACTIVE          = 1 << 10,
    CLIENT_SSL                  = 1 << 11,
    CLIENT_IGNORE_SIGPIPE       = 1 << 12,
    CLIENT_TRANSACTIONS         = 1 << 13,
    CLIENT_RESERVED             = 1 << 14,
    CLIENT_SECURE_CONNECTION    = 1 << 15,
    CLIENT_MULTI_STATEMENTS     = 1 << 16,
    CLIENT_MULTI_RESULTS        = 1 << 17,
};

enum fxmy_commands_t
{
    COM_SLEEP,
    COM_QUIT,
    COM_INIT_DB,
    COM_QUERY,
    COM_FIELD_LIST,
    COM_CREATE_DB,
    COM_DROP_DB,
    COM_REFRESH,
    COM_SHUTDOWN,
    COM_STATISTICS,
    COM_PROCESS_INFO,
    COM_CONNECT,
    COM_PROCESS_KILL,
    COM_DEBUG,
    COM_PING,
    COM_TIME,
    COM_DELAYED_INSERT,
    COM_CHANGE_USER,
    COM_BINLOG_DUMP,
    COM_TABLE_DUMP,
    COM_CONNECT_OUT,
    COM_REGISTER_SLAVE,
    COM_STMT_PREPARE,
    COM_STMT_EXECUTE,
    COM_STMT_SEND_LONG_DATA,
    COM_STMT_CLOSE,
    COM_STMT_RESET,
    COM_SET_OPTION,
    COM_STMT_FETCH,
};

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

int
fxmy_send_handshake(struct fxmy_connection_t *conn);

void
fxmy_get_handshake_packet(void **ptr, size_t *size);

#endif
