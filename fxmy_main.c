#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>

#include "fxmy_common.h"
#include "fxmy_read.h"
#include "fxmy_write.h"
#include "fxmy_conn.h"
#include "fxmy_main.h"
#include "fxmy_error.h"
#include "fxmy_string.h"
#include "fxmy_query.h"
#include "fxmy_odbc.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define snprintf _snprintf
#pragma warning(push, 0)
#include <sql.h>
#include <sqlext.h>
#pragma warning(pop)
#endif

static int
fxmy_verify_and_log_odbc(SQLRETURN return_code, SQLSMALLINT handle_type, SQLHANDLE handle)
{
    SQLCHAR sql_state_code[6];
    SQLINTEGER native_error;
    SQLCHAR message_text[1024];
    SQLSMALLINT text_length;
    SQLSMALLINT i = 1;

    if (return_code == SQL_SUCCESS)
        return 0;

    while (SQLGetDiagRecA(
        handle_type,
        handle,
        i,
        sql_state_code,
        &native_error,
        message_text,
        sizeof message_text,
        &text_length) == SQL_SUCCESS)
    {
        ++i;
        sql_state_code[5] = 0;
        message_text[1023] = 0;
        fprintf(stderr, "[fxmy] (%s) %s\n", sql_state_code, message_text);
    }

    return return_code == SQL_ERROR;
}

#define VERIFY_ODBC(return_value, handle_type, handle)                  \
    if (fxmy_verify_and_log_odbc(return_value, handle_type, handle))    \
    {                                                                   \
        __debugbreak();                                                 \
    }                                                                   \
    else (void)0

static int
fxmy_connect(struct fxmy_connection_t *conn)
{
    struct fxmy_odbc_t *odbc = conn->odbc;
    SQLHDBC database_connection_handle = odbc->database_connection_handle;
    SQLRETURN rv;
    fxmy_char connection_string_out[4096];
    SQLSMALLINT connection_string_out_length;
    fxmy_char *database_start;
    fxmy_char *database_end;

    VERIFY(conn->database && (fxmy_strlen(conn->database) > 0));

    rv = SQLBrowseConnect(
        database_connection_handle,
        (fxmy_char *)conn->connection_string,
        SQL_NTS,
        connection_string_out,
        (sizeof connection_string_out) / (sizeof connection_string_out[0]),
        &connection_string_out_length);

    /*
     * Parse the returned connection string for the database that we are supposed
     * to connect to.
     */

    database_start = fxmy_strstr(connection_string_out, C("DATABASE:Database={"));

    if (database_start == NULL)
        goto CONNECT_UNKNOWN_DATABASE;

    database_end = fxmy_strstr(database_start, C("}"));
    *database_end = 0;

    if (fxmy_strstr(database_start, conn->database) == NULL)
        goto CONNECT_UNKNOWN_DATABASE;

    goto CONNECT_SUCCESS;

CONNECT_UNKNOWN_DATABASE:
    conn->status = fxmy_get_status(FXMY_STATUS_UNKNOWN_DATABASE);

CONNECT_SUCCESS:
    __debugbreak();
    return 0;
}

static int
fxmy_parse_auth_packet(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;

    buffer->cursor = 0;

    conn->client_flags = fxmy_read_u32(buffer);
    conn->max_packet_size = fxmy_read_u32(buffer);
    conn->charset = fxmy_read_u8(buffer);
    
    /* 
     * skip over the 23-bytes of padding, as per the MySQL 4.1 wire protocol
     */
    buffer->cursor += 23;

    /*
     * skip over the user name
     */
    fxmy_skip_string(buffer);

    /*
     * skip over the scramble buffer
     */
    fxmy_skip_lcs(buffer);

    if (conn->client_flags & CLIENT_CONNECT_WITH_DB)
    {
        size_t size = strlen(buffer->memory) + 1;
        conn->database = calloc(size, sizeof(fxmy_char));
        fxmy_strfromchar(conn->database, buffer->memory, size);
        return fxmy_connect(conn);
    }

    return 0;
}

static int
fxmy_recv_auth_packet(struct fxmy_connection_t *conn)
{
    fxmy_recv(conn);
    return fxmy_parse_auth_packet(conn);
}

static int
fxmy_send_result(struct fxmy_connection_t *conn)
{
    struct fxmy_xfer_buffer_t *buffer = &conn->xfer_buffer;
    struct fxmy_xfer_buffer_t temp_buffer = { NULL, 0, 0 };
    fxmy_reset_xfer_buffer(buffer);

    if (conn->column_count_error_code == FXMY_OK)
    {
        /* 
         * success
         */
        fxmy_serialize_u8(&temp_buffer, conn->status->header);          /* OK header */
        fxmy_serialize_lcb(&temp_buffer, conn->affected_rows);          /* affected rows */
        fxmy_serialize_lcb(&temp_buffer, conn->insert_id);              /* insert id */
        fxmy_serialize_u16(&temp_buffer, 0);                            /* status flags */
        fxmy_serialize_u16(&temp_buffer, 0);                            /* warnings */
        fxmy_serialize_string(&temp_buffer, conn->status->message);     /* info  */
    }
    else if (conn->column_count_error_code == FXMY_ERROR)
    {
        /*
         * error
         */
        fxmy_serialize_u8(&temp_buffer, conn->status->header);          /* error header */
        fxmy_serialize_u16(&temp_buffer, conn->status->status_code);    /* MySQL error code */
        fxmy_serialize_string(&temp_buffer, "#");                       /* sql state marker */
        fxmy_serialize_string(&temp_buffer, conn->status->sql_state);   /* sql state (see ANSI SQL) */
        fxmy_serialize_string(&temp_buffer, conn->status->message);     /* error message */
    }
    else
    {
        /* need to return columns here */
        __debugbreak();
    }

    fxmy_send(conn, temp_buffer.memory, temp_buffer.cursor);
    fxmy_reset_xfer_buffer(&temp_buffer);

    return 0;
}

static int
fxmy_handle_command_packet(struct fxmy_connection_t *conn)
{
    uint8_t command;
    uint8_t *ptr;
    size_t packet_size_less_command;

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
        conn->database = calloc(packet_size_less_command + 1, sizeof(fxmy_char));
        fxmy_strfromchar(conn->database, (const char *)ptr, packet_size_less_command);
        return fxmy_connect(conn);

    case COM_QUERY:
        return fxmy_handle_query(conn, ptr, packet_size_less_command);

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
fxmy_reset_transient_state(struct fxmy_connection_t *conn)
{
    conn->column_count_error_code = 0;
    conn->affected_rows = 0;
    conn->insert_id = 0;
    conn->status = fxmy_get_status(FXMY_STATUS_OK);
}

void
fxmy_worker(struct fxmy_connection_t *conn)
{
    struct fxmy_odbc_t odbc_storage;
    struct fxmy_odbc_t *odbc = &odbc_storage;

    conn->odbc = odbc;

    memset(odbc, 0, sizeof *odbc);

    VERIFY_ODBC(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc->environment_handle), SQL_HANDLE_ENV, odbc->environment_handle);
    VERIFY_ODBC(SQLSetEnvAttr(odbc->environment_handle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_UINTEGER), SQL_HANDLE_ENV, odbc->environment_handle);
    VERIFY_ODBC(SQLAllocHandle(SQL_HANDLE_DBC, odbc->environment_handle, &odbc->database_connection_handle), SQL_HANDLE_ENV, odbc->environment_handle);

    VERIFY(!fxmy_send_handshake(conn));
    VERIFY(!fxmy_recv_auth_packet(conn));

    ASSERT(conn->affected_rows == 0);
    ASSERT(conn->insert_id == 0);

    conn->status = fxmy_get_status(FXMY_STATUS_OK);
    VERIFY(!fxmy_send_result(conn));

    for (;;)
    {
        int rv = fxmy_handle_command_packet(conn);

        if (!rv)
        {
            VERIFY(!fxmy_send_result(conn));
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

    VERIFY_ODBC(SQLDisconnect(odbc->database_connection_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);
    VERIFY_ODBC(SQLFreeHandle(SQL_HANDLE_DBC, odbc->database_connection_handle), SQL_HANDLE_DBC, odbc->database_connection_handle);
    VERIFY_ODBC(SQLFreeHandle(SQL_HANDLE_ENV, odbc->environment_handle), SQL_HANDLE_ENV, odbc->environment_handle);

    conn->odbc = NULL;
    fxmy_conn_dispose(conn);
}
