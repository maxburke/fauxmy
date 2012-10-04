#include <stdlib.h>
#include <ctype.h>

#include "fxmy_common.h"
#include "fxmy_core.h"
#include "fxmy_string.h"

#define FXMY_STRISTR_BODY(type, lower_func) \
    const type *needle_ptr;     \
    const type *haystack_ptr;   \
    type h;                     \
    type n;                     \
                                \
    if (haystack == NULL || needle == NULL) \
        return NULL;            \
                                \
    do                          \
    {                           \
        haystack_ptr = haystack;\
        needle_ptr = needle;    \
        h = *haystack_ptr;      \
        n = *needle_ptr;        \
                                \
        while (lower_func(h) == lower_func(n))    \
        {                       \
            /*                  \
             * the n == 0 case here is implied as the inner body of the loop is \
             * only entered if h == n.  \
             */                 \
            if (h == 0)         \
                break;          \
                                \
            h = *++haystack_ptr;\
            n = *++needle_ptr;  \
        }                       \
                                \
        if (*needle_ptr == 0)   \
            return haystack;    \
    }                           \
    while (*haystack++ != 0);   \
                                \
    return NULL;                \

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
fxmy_fstrfromchar(fxmy_char *dest, const char *src, size_t size)
{
    const char * const end = src + size;
    size_t i;

    memset(dest, 0, size * sizeof(fxmy_char));

    for (i = 0; src < end; ++i) 
        dest[i] = fxmy_utf8_to_ucs2(src, &src);

    return dest;
}

size_t
fxmy_fstrlenfromchar(const char *src, size_t size)
{
    const char * const end = src + size;
    size_t i;

    for (i = 0; src < end; ++i)
        fxmy_utf8_to_ucs2(src, &src);

    return i;
}

fxmy_char *
fxmy_fstrncatfromchar(fxmy_char *dest, size_t size, const char *src, size_t num_chars)
{
    fxmy_char *p;
    size_t i;
    size_t amount_to_copy;

    for (i = 0, p = dest; i < (size - 1) && *p != 0; ++i, ++p)
        ;

    if (i == (size - 1))
    {
        dest[size - 1] = 0;
        return NULL;
    }

    amount_to_copy = MIN(size - i, num_chars);

    p = fxmy_fstrfromchar(dest + i, src, amount_to_copy);

    dest[size - 1] = 0;
    return p;
}

const fxmy_char *
fxmy_fstristr(const fxmy_char *haystack, const fxmy_char * const needle)
{
    FXMY_STRISTR_BODY(fxmy_char, towlower)
}


#endif

const char *
fxmy_stristr(const char *haystack, const char * const needle)
{
    FXMY_STRISTR_BODY(char, tolower)
}

