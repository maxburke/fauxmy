#include <stdlib.h>
#include <string.h>

#include "fxmy_common.h"
#include "fxmy_read.h"
#include "fxmy_write.h"
#include "fxmy_conn.h"
#include "fxmy_main.h"

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
    fxmy_recv(conn);
    fxmy_parse_auth_packet(conn);
    return 0;
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
fxmy_reset_transient_state(struct fxmy_connection_t *conn)
{
    conn->affected_rows = 0;
    conn->insert_id = 0;
    conn->query_message = NULL;
}

void
fxmy_worker(void)
{
    for (;;)
    {
        struct fxmy_connection_t *conn = fxmy_conn_create();

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

		fxmy_conn_dispose(conn);
    }
}
