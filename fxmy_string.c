#include <stdlib.h>

#include "fxmy_common.h"
#include "fxmy_string.h"

#ifdef _MSC_VER

static fxmy_char
fxmy_utf8_to_ucs2(const char *input, const char **output)
{
    const unsigned char *in = (const unsigned char *)input;
    const unsigned char **out = (const unsigned char **)output;

    if (in[0] == 0)
        return 0;

    if (in[0] < 0x80)
    {
        *out = in + 1;
        return (fxmy_char)in[0];
    }

    if ((in[0] & 0xE0) == 0xE0)
    {
        ASSERT(in[1] != 0 || in[2] != 0);
        *out = in + 3;

        return ((in[0] & 0xF) << 12) | ((in[1] & 0x3F) << 6) | (in[2] & 0x3F);
    }

    if ((in[0] & 0xC0) == 0xC0)
    {
        ASSERT(in[1] != 0);
        *out = in + 2;

        return ((in[0] & 0x1F) << 6) | (in[1] & 0x3F);
    }

    abort();
    return 0;
}

fxmy_char *
fxmy_strfromchar(fxmy_char *dest, const char *src, size_t num_chars)
{
    const char * const end = src + num_chars;
    size_t i;

    memset(dest, 0, num_chars * sizeof(fxmy_char));

    for (i = 0; src < end; ++i) 
        dest[i] = fxmy_utf8_to_ucs2(src, &src);
    
    return dest;
}

#endif
