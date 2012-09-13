#ifndef FXMY_READ_H
#define FXMY_READ_H

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4668)
#endif

#include <stdint.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct fxmy_xfer_buffer_t;

uint8_t
fxmy_read_u8(struct fxmy_xfer_buffer_t *buffer);

uint32_t
fxmy_read_u32(struct fxmy_xfer_buffer_t *buffer);

uint64_t
fxmy_read_lcb(struct fxmy_xfer_buffer_t *buffer);

const char *
fxmy_read_lcs(struct fxmy_xfer_buffer_t *buffer);

const char *
fxmy_read_string(struct fxmy_xfer_buffer_t *buffer);

void
fxmy_skip_string(struct fxmy_xfer_buffer_t *buffer);

void
fxmy_skip_lcs(struct fxmy_xfer_buffer_t *buffer);


#endif