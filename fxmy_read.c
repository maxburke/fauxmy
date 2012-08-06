#include <stdlib.h>

#include "fxmy_read.h"
#include "fxmy_common.h"

uint8_t
fxmy_read_u8(struct fxmy_xfer_buffer_t *buffer)
{
    const uint8_t *ptr = (const uint8_t *)buffer->memory + buffer->cursor;
    ++buffer->cursor;
    VERIFY(buffer->cursor <= buffer->size);
    
    return ptr[0];
}

uint32_t
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

uint64_t
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

const char *
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

const char *
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

void
fxmy_skip_string(struct fxmy_xfer_buffer_t *buffer)
{
    const char *ptr = (const char *)buffer->memory + buffer->cursor;
    size_t size = strlen(ptr) + 1;
    buffer->cursor += size;
}

void
fxmy_skip_lcs(struct fxmy_xfer_buffer_t *buffer)
{
    size_t size = (size_t)fxmy_read_lcb(buffer);
    buffer->cursor += size;
}
