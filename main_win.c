#pragma warning(push, 0)
#include <stdlib.h>
#include <stdio.h>
#pragma warning(pop)
#pragma warning(disable:4514)

#include "fxmy_common.h"

#pragma comment(lib, "Ws2_32.lib")

#define FXMY_NUM_THREADS 4
#define FXMY_DEFAULT_PORT 3306
#define FXMY_DEFAULT_LISTEN_BACKLOG 50

static HANDLE fxmy_completion_port;
static SOCKET fxmy_listen_socket;

void
fxmy_perror(const char *string)
{
    DWORD error;
    DWORD wsa_error;
    size_t string_length;
    size_t i;
    static TCHAR buffer[1024];
    
    fprintf(stderr, "%s\n", string);
    string_length = strlen(string);
    for (i = 0; i < string_length; ++i)
        fprintf(stderr, "-");
    fprintf(stderr, "\n");

    error = GetLastError();
    if (error != NO_ERROR)
    {
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            0,
            buffer,
            1024,
            NULL);

        fwprintf(stderr, L"%s\n", buffer);
    }

    wsa_error = WSAGetLastError();
    if (wsa_error != error && wsa_error != NO_ERROR)
    {
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            wsa_error,
            0,
            buffer,
            1024,
            NULL);

        fwprintf(stderr, L"%s\n", buffer);
    }
}

int
fxmy_send(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    int bytes_written;

    bytes_written = send(conn->socket, buffer->memory, buffer->size, 0);

    VERIFY((size_t)bytes_written == buffer->size);

    fxmy_reset_xfer_buffer(buffer);

    return 0;
}

int
fxmy_recv(struct fxmy_connection_t *conn)
{
    int bytes_read;

    bytes_read = recv(conn->socket, conn->xfer_buffer.memory, (int)conn->xfer_buffer.size, 0);
    VERIFY((size_t)bytes_read == conn->xfer_buffer.size);

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

    /* conn->packet_number holds the value of the next packet we operate on
       so we validate that the packet we've received has the expected ID,
       then increment it so that it is correct when we send the response. */
    conn->packet_number = packet_number + 1;

    conn->xfer_buffer.memory = malloc(packet_size);
    conn->xfer_buffer.size += packet_size;
}

static uint32_t
fxmy_read_u32(struct fxmy_xfer_buffer_t *buffer)
{
    const uint8_t *ptr = (const uint8_t *)buffer->memory + buffer->cursor;
    uint32_t value = ((uint32_t)ptr[0])
        | ((uint32_t)ptr[1] << 8)
        | ((uint32_t)ptr[2] << 16)
        | ((uint32_t)ptr[3] << 24);
    buffer->cursor += 4;
    VERIFY(buffer->cursor <= buffer->size);

    return value;
}

static uint8_t
fxmy_read_u8(struct fxmy_xfer_buffer_t *buffer)
{
    const uint8_t *ptr = (const uint8_t *)buffer->memory + buffer->cursor;
    ++buffer->cursor;
    VERIFY(buffer->cursor <= buffer->size);
    
    return ptr[0];
}

static uint64_t
fxmy_read_lcb(struct fxmy_xfer_buffer_t *buffer)
{
    const uint8_t *ptr = (const uint8_t *)buffer->memory + buffer->cursor;
    const uint8_t byte = ptr[0];
    uint64_t value = 0;

    switch (byte)
    {
    case 254:
        buffer->cursor += 9;
        value = ((uint64_t)ptr[1])
            | ((uint64_t)ptr[2] << 8)
            | ((uint64_t)ptr[3] << 16)
            | ((uint64_t)ptr[4] << 24)
            | ((uint64_t)ptr[5] << 32)
            | ((uint64_t)ptr[6] << 40)
            | ((uint64_t)ptr[7] << 48)
            | ((uint64_t)ptr[8] << 56);
    case 253:
        buffer->cursor += 4;
        value = ((uint64_t)ptr[1])
            | ((uint64_t)ptr[2] << 8)
            | ((uint64_t)ptr[3] << 16);
    case 252:
        buffer->cursor += 3;
        value = ((uint64_t)ptr[1])
            | ((uint64_t)ptr[2] << 8);
    case 251:
        /* 251 signifies a NULL column value and will never be read. */
    default:
        ++buffer->cursor;
        value = (uint64_t)byte;
    }

    VERIFY(buffer->cursor <= buffer->size);
    return value;
}

static const char *
fxmy_read_lcs(struct fxmy_xfer_buffer_t *buffer)
{
    char *memory;
    const char *ptr = (const char *)buffer->memory + buffer->cursor;
    size_t size = (size_t)fxmy_read_lcb(buffer);

    memory = calloc(size + 1, 1);
    memcpy(buffer->memory, ptr, size);
    buffer->cursor += size;
    VERIFY(buffer->cursor <= buffer->size);

    return memory;
}

static const char *
fxmy_read_string(struct fxmy_xfer_buffer_t *buffer)
{
    const char *ptr = (const char *)buffer->memory + buffer->cursor;
    size_t size = strlen(ptr) + 1;
    char *string = calloc(size, 1);
    memcpy(string, ptr, size);

    buffer->cursor += size;
    VERIFY(buffer->cursor <= buffer->size);

    return string;
}

static void
fxmy_skip_string(struct fxmy_xfer_buffer_t *buffer)
{
    const char *ptr = (const char *)buffer->memory + buffer->cursor;
    size_t size = strlen(ptr) + 1;
    buffer->cursor += size;
}

static void
fxmy_skip_lcs(struct fxmy_xfer_buffer_t *buffer)
{
    size_t size = (size_t)fxmy_read_lcb(buffer);
    buffer->cursor += size;
}

static void
fxmy_parse_auth_packet(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;

    buffer->cursor = 0;

    conn->client_flags = fxmy_read_u32(buffer);
    conn->max_packet_size = fxmy_read_u32(buffer);
    conn->charset = fxmy_read_u8(buffer);
    
    /* skip over the 23-bytes of padding, as per the MySQL 4.1 wire protocol */
    buffer->cursor += 23;
    /* skip over the user name */
    fxmy_skip_string(buffer);
    /* skip over the scramble buffer */
    fxmy_skip_lcs(buffer);

    if (conn->client_flags & CLIENT_CONNECT_WITH_DB)
    {
        size_t size = strlen(buffer->memory) + 1;
        conn->database = calloc(size, 1);
        memcpy(conn->database, buffer->memory, size);
    }
}

static int
fxmy_recv_auth_packet(struct fxmy_connection_t *conn)
{
    fxmy_begin_recv(conn);
    fxmy_recv(conn);
    fxmy_parse_auth_packet(conn);
    return 0;
}

static void
fxmy_ensure_buffer_room(struct fxmy_xfer_buffer_t *buffer, size_t size)
{
    size_t old_size = buffer->size;
    size_t headroom = old_size - buffer->cursor;
    const size_t new_size = (old_size + size + 4095) & (~4095);

    if (size < headroom)
        return;

    buffer->memory = realloc(buffer->memory, new_size);
    buffer->size = new_size;
}

static void
fxmy_serialize_lcb(struct fxmy_xfer_buffer_t *buffer, uint64_t value)
{
    uint8_t *dest;

    fxmy_ensure_buffer_room(buffer, 9);
    dest = (uint8_t *)buffer->memory + buffer->cursor;
    
    if (value <= 250)
    {
        *dest = (uint8_t)value;

        buffer->cursor += 1;
    }
    else if (value <= ((1 << 16) - 1))
    {
        *dest++ = 252;
        *dest++ = (uint8_t)(value & 0xFF);
        *dest++ = (uint8_t)((value >> 8) & 0xFF);

        buffer->cursor += 3;
    }
    else if (value <= ((1 << 24) - 1))
    {
        *dest++ = 253;
        *dest++ = (uint8_t)(value & 0xFF);
        *dest++ = (uint8_t)((value >> 8) & 0xFF);
        *dest++ = (uint8_t)((value >> 16) & 0xFF);

        buffer->cursor += 4;
    }
    else
    {
        *dest++ = 254;
        *dest++ = (uint8_t)(value & 0xFF);
        *dest++ = (uint8_t)((value >> 8) & 0xFF);
        *dest++ = (uint8_t)((value >> 16) & 0xFF);
        *dest++ = (uint8_t)((value >> 24) & 0xFF);
        *dest++ = (uint8_t)((value >> 32) & 0xFF);
        *dest++ = (uint8_t)((value >> 40) & 0xFF);
        *dest++ = (uint8_t)((value >> 48) & 0xFF);
        *dest++ = (uint8_t)((value >> 56) & 0xFF);

        buffer->cursor += 9;
    }
}

static void
fxmy_serialize_u16(struct fxmy_xfer_buffer_t *buffer, uint16_t value)
{
    uint8_t *dest;

    fxmy_ensure_buffer_room(buffer, 2);
    dest = (uint8_t *)buffer->memory + buffer->cursor;

    *dest++ = (uint8_t)(value & 0xFF);
    *dest++ = (uint8_t)((value >> 8) & 0xFF);

    buffer->cursor += 2;
}

static void
fxmy_serialize_u8(struct fxmy_xfer_buffer_t *buffer, uint8_t value)
{
    uint8_t *dest;

    fxmy_ensure_buffer_room(buffer, 1);
    dest = (uint8_t *)buffer->memory + buffer->cursor;

    *dest++ = value;

    buffer->cursor += 1;
}

static void
fxmy_serialize_terminated_string(struct fxmy_xfer_buffer_t *buffer, const char *value)
{
    size_t string_bytes;
    if (value == NULL)
        return;
    
    string_bytes = strlen(value) + 1;

    fxmy_ensure_buffer_room(buffer, string_bytes);
    memcpy((char *)buffer->memory + buffer->cursor, value, string_bytes);
    buffer->cursor += string_bytes;
}

static void
fxmy_serialize_string(struct fxmy_xfer_buffer_t *buffer, const char *value)
{
    size_t string_length;

    if (value == NULL)
        return;

    string_length = strlen(value);

    fxmy_ensure_buffer_room(buffer, string_length);
    memcpy((char *)buffer->memory + buffer->cursor, value, string_length);
    buffer->cursor += string_length;
}



static int
fxmy_send_ok_packet(struct fxmy_connection_t *conn, uint64_t affected_rows, uint64_t insert_id, const char *message)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    struct fxmy_xfer_buffer_t temp_buffer = { NULL, 0, 0 };
    fxmy_reset_xfer_buffer(buffer);

    fxmy_serialize_u8(&temp_buffer, 0);
    fxmy_serialize_lcb(&temp_buffer, affected_rows);
    fxmy_serialize_lcb(&temp_buffer, insert_id);
    fxmy_serialize_u16(&temp_buffer, 0);
    fxmy_serialize_u16(&temp_buffer, 0);
    fxmy_serialize_string(&temp_buffer, message);

    fxmy_begin_send(conn, temp_buffer.memory, temp_buffer.cursor);
    fxmy_send(conn);
    fxmy_reset_xfer_buffer(&temp_buffer);

    return 0;
}

static int
fxmy_handle_command_packet(struct fxmy_connection_t *conn)
{
    uint8_t command;
    uint8_t *ptr;
    size_t packet_size_less_command;

    fxmy_begin_recv(conn);
    fxmy_recv(conn);

    ptr = conn->xfer_buffer.memory;
    command = *ptr++;
    packet_size_less_command = conn->xfer_buffer.size - sizeof command;

    switch (command)
    {
    case COM_SLEEP:
        abort();
        return 0;

    case COM_QUIT:
        return -1;

    case COM_INIT_DB:
        if (conn->database)
            free(conn->database);
        conn->database = calloc(packet_size_less_command + 1, 1);
        memcpy(conn->database, conn->xfer_buffer.memory, packet_size_less_command);
        return 0;

    case COM_QUERY:
    case COM_FIELD_LIST:
    case COM_CREATE_DB:
    case COM_DROP_DB:
    case COM_REFRESH:
    case COM_SHUTDOWN:
    case COM_STATISTICS:
    case COM_PROCESS_INFO:
    case COM_CONNECT:
    case COM_PROCESS_KILL:
    case COM_DEBUG:
    case COM_PING:
    case COM_TIME:
    case COM_DELAYED_INSERT:
    case COM_CHANGE_USER:
    case COM_BINLOG_DUMP:
    case COM_TABLE_DUMP:
    case COM_CONNECT_OUT:
    case COM_REGISTER_SLAVE:
    case COM_STMT_PREPARE:
    case COM_STMT_EXECUTE:
    case COM_STMT_SEND_LONG_DATA:
    case COM_STMT_CLOSE:
    case COM_STMT_RESET:
        abort();
        return 0;

    case COM_SET_OPTION:
        {
            const uint16_t *option = (const uint16_t *)ptr;
            conn->multi_statements_off = *option;
            return 0;
        }

    case COM_STMT_FETCH:
        abort();
        return 0;


    default:
        abort();
    }

    return 1;
}

static void
fxmy_socket_open(unsigned short port)
{
    WSADATA wsa_data;
    struct sockaddr_in addr;
    int rv;

    rv = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    VERIFY(rv == NO_ERROR);
        
    fxmy_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    VERIFY(fxmy_listen_socket != INVALID_SOCKET);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    rv = bind(fxmy_listen_socket, (SOCKADDR *)&addr, sizeof addr);
    VERIFY(rv != SOCKET_ERROR);

    rv = listen(fxmy_listen_socket, FXMY_DEFAULT_LISTEN_BACKLOG);
    VERIFY(rv != SOCKET_ERROR);
}

static void
fxmy_reset_transient_state(struct fxmy_connection_t *conn)
{
    conn->affected_rows = 0;
    conn->insert_id = 0;
    conn->query_message = NULL;
}

static DWORD WINAPI
fxmy_worker_thread(LPVOID parameter)
{
    UNUSED(parameter);

    for (;;)
    {
        DWORD num_bytes;
        BOOL result;
        ULONG_PTR completion_key;
        OVERLAPPED *overlapped;
        struct fxmy_connection_t *conn;

        result = GetQueuedCompletionStatus(
            fxmy_completion_port,
            &num_bytes,
            &completion_key,
            &overlapped,
            INFINITE);

        VERIFY(result);

        conn = (struct fxmy_connection_t *)overlapped;

        VERIFY(!fxmy_send_handshake(conn));
        VERIFY(!fxmy_recv_auth_packet(conn));
        VERIFY(!fxmy_send_ok_packet(conn, 0, 0, NULL));

        for (;;)
        {
            int rv = fxmy_handle_command_packet(conn);

            if (!rv)
            {
                VERIFY(!fxmy_send_ok_packet(conn, conn->affected_rows, conn->insert_id, conn->query_message));
                fxmy_reset_transient_state(conn);
            }
            else if (rv < 0)
            {
                break;
            }
            else
            {
            }
        }

        CloseHandle((HANDLE)conn->socket);
        free(conn);
    }
}

int
main(void)
{
    HANDLE thread_handles[FXMY_NUM_THREADS];
    int i;

    fxmy_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, FXMY_NUM_THREADS);
    VERIFY(fxmy_completion_port != NULL);

    fxmy_socket_open(FXMY_DEFAULT_PORT);

    for (i = 0; i < FXMY_NUM_THREADS; ++i)
        thread_handles[i] = CreateThread(
            NULL,
            0,
            fxmy_worker_thread,
            NULL,
            0,
            NULL);

    for (;;)
    {
        SOCKET new_connection;
        struct sockaddr_in addr;
        int size = sizeof addr;
        struct fxmy_connection_t *conn = calloc(1, sizeof(struct fxmy_connection_t));

        new_connection = accept(fxmy_listen_socket, (struct sockaddr *)&addr, &size);
        conn->socket = new_connection;

        PostQueuedCompletionStatus(fxmy_completion_port, 0, 1, &conn->overlapped);
    }
}
