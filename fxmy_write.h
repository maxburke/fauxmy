#ifndef FXMY_WRITE_H
#define FXMY_WRITE_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4668)
#endif

#include <stdint.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct fxmy_xfer_buffer_t;

void
fxmy_serialize_lcb(struct fxmy_xfer_buffer_t *buffer, uint64_t value);

void
fxmy_serialize_u16(struct fxmy_xfer_buffer_t *buffer, uint16_t value);

void
fxmy_serialize_u8(struct fxmy_xfer_buffer_t *buffer, uint8_t value);

void
fxmy_serialize_terminated_string(struct fxmy_xfer_buffer_t *buffer, const char *value);

void
fxmy_serialize_string(struct fxmy_xfer_buffer_t *buffer, const char *value);

#endif
