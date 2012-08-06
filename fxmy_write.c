#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_write.h"

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

void
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

void
fxmy_serialize_u16(struct fxmy_xfer_buffer_t *buffer, uint16_t value)
{
    uint8_t *dest;

    fxmy_ensure_buffer_room(buffer, 2);
    dest = (uint8_t *)buffer->memory + buffer->cursor;

    *dest++ = (uint8_t)(value & 0xFF);
    *dest++ = (uint8_t)((value >> 8) & 0xFF);

    buffer->cursor += 2;
}

void
fxmy_serialize_u8(struct fxmy_xfer_buffer_t *buffer, uint8_t value)
{
    uint8_t *dest;

    fxmy_ensure_buffer_room(buffer, 1);
    dest = (uint8_t *)buffer->memory + buffer->cursor;

    *dest++ = value;

    buffer->cursor += 1;
}

void
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

void
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

