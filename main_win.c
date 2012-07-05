
#pragma warning(push, 0)
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <stdio.h>
#pragma warning(pop)

#include "fxmy_common.h"

#pragma comment(lib, "Ws2_32.lib")

#define FXMY_DEFAULT_PORT 3306
#define FXMY_DEFAULT_LISTEN_BACKLOG 50
#define FXMY_POISON_PILL 0xDEADBEEF
#define FXMY_SOCKET_KEY 1
#define FXMY_CONNECTION_KEY 2
#define FXMY_NUM_THREADS 4

static HANDLE fxmy_completion_port;
static SOCKET fxmy_listen_socket;
static LPFN_ACCEPTEX AcceptExFn;

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
    BOOL rv;
    void *start;
    size_t size;

    VERIFY(fxmy_xfer_in_progress(buffer));
    VERIFY(buffer->size > 0);
    VERIFY(buffer->size > buffer->cursor);

    buffer->cursor += conn->bytes_transferred;

    memset(&conn->overlapped, 0, sizeof conn->overlapped);

    if (buffer->cursor == buffer->size)
    {
        fxmy_reset_xfer_buffer(buffer);
        return 1;
    }

    start = (char *)buffer->memory + buffer->cursor;
    size = buffer->size - buffer->cursor;
    rv = WriteFile(
        (HANDLE)conn->socket, 
        start,
        size,
        NULL,
        &conn->overlapped);

    VERIFY(rv == TRUE || (rv == FALSE && GetLastError() == ERROR_IO_PENDING));
   
    conn->bytes_transferred = 0;
    return 0;
}

static void
fxmy_socket_open(int port)
{
    WSADATA wsa_data;
    struct sockaddr_in addr;
    int rv;
    GUID accept_ex_guid = WSAID_ACCEPTEX;
    DWORD bytes;
    HANDLE new_iocp;

    rv = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    VERIFY(rv == NO_ERROR);
        
    fxmy_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    VERIFY(fxmy_listen_socket != INVALID_SOCKET);

    new_iocp = CreateIoCompletionPort((HANDLE)fxmy_listen_socket, fxmy_completion_port, FXMY_SOCKET_KEY, 0);
    VERIFY(new_iocp == fxmy_completion_port);
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    rv = bind(fxmy_listen_socket, (SOCKADDR *)&addr, sizeof addr);
    VERIFY(rv != SOCKET_ERROR);

    rv = listen(fxmy_listen_socket, FXMY_DEFAULT_LISTEN_BACKLOG);
    VERIFY(rv != SOCKET_ERROR);

    rv = WSAIoctl(
        fxmy_listen_socket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &accept_ex_guid,
        sizeof accept_ex_guid,
        &AcceptExFn,
        sizeof AcceptExFn,
        &bytes,
        NULL,
        NULL);
    VERIFY(rv != SOCKET_ERROR);
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

        if (completion_key == FXMY_POISON_PILL)
            return 0;

        conn = (struct fxmy_connection_t *)overlapped;
        conn->bytes_transferred = num_bytes;
        conn->callback(conn);
    }
}

static int
fxmy_accept_callback(struct fxmy_connection_t *conn)
{
    fxmy_reset_xfer_buffer(&conn->xfer_buffer);
    conn->callback = fxmy_send_handshake;

    return fxmy_send_handshake(conn);
}

static struct fxmy_connection_t *
fxmy_create_connection(SOCKET socket)
{
    struct fxmy_connection_t *conn;
    const size_t alloc_size = 2 * (sizeof(struct sockaddr_in) + 16);
    
    conn = calloc(sizeof(struct fxmy_connection_t), 1);
    conn->xfer_buffer.memory = calloc(alloc_size, 1);
    conn->xfer_buffer.size = alloc_size;
    conn->socket = socket;
    conn->callback = fxmy_accept_callback;

    return conn;
}

int
main(void)
{
    HANDLE thread_handles[FXMY_NUM_THREADS] = {0};
    WSAEVENT accept_event;
    int i;
    int rv;

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

    accept_event = WSACreateEvent();
    rv = WSAEventSelect(fxmy_listen_socket, accept_event, FD_ACCEPT);
    VERIFY(rv != SOCKET_ERROR);

    for (;;)
    {
        DWORD bytes;
        BOOL rv;
        SOCKET accept_socket;
        struct fxmy_connection_t *conn;

        accept_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        VERIFY(accept_socket != INVALID_SOCKET);

        conn = fxmy_create_connection(accept_socket);

        rv = AcceptExFn(
            fxmy_listen_socket,
            accept_socket,
            conn->xfer_buffer.memory,
            0,
            sizeof(struct sockaddr_in) + 16,
            sizeof(struct sockaddr_in) + 16,
            &bytes,
            &conn->overlapped);

        if (rv == FALSE)
        {
            VERIFY(WSAGetLastError() == ERROR_IO_PENDING);

            if (WSAWaitForMultipleEvents(1, &accept_event, FALSE, INFINITE, FALSE) != WSA_WAIT_TIMEOUT)
            {
                WSANETWORKEVENTS event;
                rv = WSAEnumNetworkEvents(fxmy_listen_socket, accept_event, &event);
                VERIFY(rv != SOCKET_ERROR);

                if ((event.lNetworkEvents & FD_ACCEPT) && event.iErrorCode[FD_ACCEPT_BIT] == 0)
                {
                    CreateIoCompletionPort((HANDLE)conn->socket, fxmy_completion_port, FXMY_CONNECTION_KEY, 0);
                }

                WSAResetEvent(accept_event);
            }
        }
        else
        {
            PostQueuedCompletionStatus(fxmy_completion_port, bytes, FXMY_CONNECTION_KEY, &conn->overlapped);
        }
    }
}