/*
#ifdef _MSC_VER
    #pragma warning(push, 0)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <WinSock2.h>
    #include <MSWSock.h>
    #include <stdint.h>
    #pragma warning(pop)
    #pragma warning(disable:4514)
    #pragma warning(disable:4820)
*/

#pragma warning(push, 0)
#include <stdlib.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <MSWSock.h>
#pragma warning(pop)
#pragma warning(disable:4514)
#pragma warning(disable:4127)

#include "fxmy_common.h"
#include "fxmy_read.h"
#include "fxmy_write.h"
#include "fxmy_conn.h"
#include "fxmy_main.h"

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

static DWORD WINAPI
fxmy_worker_thread(LPVOID parameter)
{
	UNUSED(parameter);

	fxmy_worker();

	return 0;
}

struct fxmy_connection_t *
fxmy_conn_create(void)
{
	BOOL result;
	DWORD num_bytes;
	ULONG_PTR completion_key;
	OVERLAPPED *overlapped;

    result = GetQueuedCompletionStatus(
        fxmy_completion_port,
        &num_bytes,
        &completion_key,
        &overlapped,
        INFINITE);

	VERIFY(result);

	return (struct fxmy_connection_t *)overlapped;
}

void
fxmy_conn_dispose(struct fxmy_connection_t *conn)
{
    CloseHandle((HANDLE)conn->socket);
    free(conn);
}

/*
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

    }
}

*/
int
main(void)
{
    HANDLE thread_handles[FXMY_NUM_THREADS];
    int i;

	VERIFY(sizeof(OVERLAPPED) == OVERLAPPED_SIZE);
	VERIFY(sizeof(SOCKET) == sizeof(socket_t));

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

        PostQueuedCompletionStatus(fxmy_completion_port, 0, 1, (LPOVERLAPPED)&conn->overlapped[0]);
    }
}
