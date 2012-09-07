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
fxmy_worker_thread(LPVOID context)
{
    fxmy_worker(context);

    return 0;
}

void
fxmy_conn_dispose(struct fxmy_connection_t *conn)
{
    CloseHandle((HANDLE)conn->socket);
    conn->done = 1;
}

static void
fxmy_add_new_connection(struct fxmy_connection_t *conn, HANDLE thread_handle)
{
    struct fxmy_connection_thread_info_t 
    {
        struct fxmy_connection_t *conn;
        HANDLE thread_handle;
    };

    static struct fxmy_connection_thread_info_t *context_array;
    static size_t num_active_context_array_entries;
    static size_t num_context_array_entries;
    size_t i;

    /*
     * Scan the list of active connections and see if any are done. If the connection
     * is done, wait for the thread to exit and then release its context slot.
     */
    for (i = 0; i < num_active_context_array_entries; ++i)
    {
        struct fxmy_connection_t *current_connection = context_array[i].conn;
        if (current_connection->done)
        {
            WaitForSingleObject(context_array[i].thread_handle, INFINITE);
            free(current_connection);

            memset(&context_array[i], 0, sizeof(struct fxmy_connection_thread_info_t));
            --num_active_context_array_entries;
        }
    }

    /*
     * Realloc the array if there aren't enough entries in it
     */
    if (num_active_context_array_entries + 1 > num_context_array_entries)
    {
        const size_t ALLOCATION_DELTA = 10;
        const size_t new_num_entries = num_context_array_entries + ALLOCATION_DELTA;
        context_array = realloc(context_array, new_num_entries * sizeof(struct fxmy_connection_thread_info_t));

        /*
         * The code below that inserts the connection into the connection array
         * assumes that an available slot has its struct fxmy_connection_t * 
         * member as NULL and the thread handle is invalid. This memset ensures
         * that the memory returned by realloc is initialized properly.
         */
        memset(&context_array[num_active_context_array_entries], 0, ALLOCATION_DELTA * sizeof(struct fxmy_connection_thread_info_t));
        num_context_array_entries = new_num_entries;
    }

    for (i = 0; i < num_context_array_entries; ++i)
    {
        if (context_array[i].conn == NULL)
        {
            VERIFY(context_array[i].thread_handle == NULL);
            context_array[i].conn = conn;
            context_array[i].thread_handle = thread_handle;
            ++num_active_context_array_entries;

            return;
        }
    }

    VERIFY(0);
}

static struct fxmy_connection_t *
fxmy_create_connection(SOCKET new_connection, const char *connection_string)
{
    struct fxmy_connection_t *conn = calloc(1, sizeof(struct fxmy_connection_t));
    conn->socket = new_connection;
    conn->connection_string = connection_string;

    return conn;
}

int
main(void)
{
    /*
     * TODO: Use ini files for the connection string
     */

    const char connection_string[] = "Driver={SQL Server Native Client 11.0};Server=.\\SQLEXPRESS;UID=max;PWD=W0zixege";

    VERIFY(sizeof(SOCKET) == sizeof(socket_t));

    fxmy_socket_open(FXMY_DEFAULT_PORT);

    for (;;)
    {
        SOCKET new_connection;
        struct sockaddr_in addr;
        int size = (int)(sizeof addr);
        struct fxmy_connection_t *conn;
        HANDLE thread_handle;

        new_connection = accept(fxmy_listen_socket, (struct sockaddr *)&addr, &size);
        conn = fxmy_create_connection(new_connection, connection_string);
        
        thread_handle = CreateThread(NULL,
            0,
            fxmy_worker_thread,
            conn,
            0,
            NULL);

        fxmy_add_new_connection(conn, thread_handle);
    }
}
