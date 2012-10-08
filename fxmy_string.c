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
    return NULL;

#define FXMY_NEXT_TOKEN_BODY(type, consume_whitespace_func, find_end_of_string_func, isspace_func) \
    const type *start;                                          \
    const type *end;                                            \
    type c;                                                     \
                                                                \
    start = consume_whitespace_func(str);                       \
    end = NULL;                                                 \
                                                                \
    if (start[0] == C('-') && start[1] == C('-'))               \
    {                                                           \
        while (*start && *start != C('\n'))                     \
            ++start;                                            \
    }                                                           \
                                                                \
    start = consume_whitespace_func(str);                       \
    end = start;                                                \
    c = *start;                                                 \
                                                                \
    if (c == 0)                                                 \
        goto NO_TOKEN;                                          \
                                                                \
    if (c == C('\'') || c == C('"'))                            \
    {                                                           \
        end = find_end_of_string_func(start, c);                \
        goto GOT_TOKEN;                                         \
    }                                                           \
                                                                \
    for (;;)                                                    \
    {                                                           \
        c = *++end;                                             \
        if (isspace_func(c) || c == ';' || c == ',' || c == 0)  \
            goto GOT_TOKEN;                                     \
    }                                                           \
                                                                \
GOT_TOKEN:                                                      \
    *end_ptr = end;                                             \
    return start;                                               \
                                                                \
NO_TOKEN:                                                       \
    *end_ptr = NULL;                                            \
    return NULL;

#define FXMY_FIND_END_OF_STRING_BODY(type)                      \
    type c;                                                     \
    type prev;                                                  \
                                                                \
    prev = *start;                                              \
    while ((c = *++start) != 0)                                 \
    {                                                           \
        if (c == string_char && prev != C('\\'))                \
            return start + 1;                                   \
                                                                \
        prev = c;                                               \
    }                                                           \
                                                                \
    return start;                                               \

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
    fxmy_fstrncat(fxmy_char *dest, size_t size, const fxmy_char *src)
    {
        wcsncat(dest, src, size);

        dest[size - 1] = 0;
        return dest;
    }

    const fxmy_char *
    fxmy_fstristr(const fxmy_char *haystack, const fxmy_char * const needle)
    {
        FXMY_STRISTR_BODY(fxmy_char, towlower)
    }

    const fxmy_char *
    fxmy_fconsume_whitespace(const fxmy_char *ptr)
    {
        if (*ptr == 0)
            return ptr;

        while (iswspace(*ptr))
            ++ptr;

        return ptr;
    }

    const fxmy_char *
    fxmy_ffind_end_of_string(const fxmy_char *start, fxmy_char string_char)
    {
        FXMY_FIND_END_OF_STRING_BODY(fxmy_char)
    }

    const fxmy_char *
    fxmy_fnext_token(const fxmy_char **end_ptr, const fxmy_char *str)
    {
        FXMY_NEXT_TOKEN_BODY(fxmy_char, fxmy_fconsume_whitespace, fxmy_ffind_end_of_string, iswspace)
    }

    int
    fxmy_fstrnicmp(const fxmy_char *lhs, const fxmy_char *rhs, size_t length)
    {
        size_t i;

        for (i = 0; i < length; ++i)
        {
            const fxmy_char a = towlower(lhs[i]);
            const fxmy_char b = towlower(rhs[i]);

            if (a != b)
                return a - b;
        }

        return 0;
    }


#else
    fxmy_char *
    fxmy_fstrncat(fxmy_char *dest, size_t size, const fxmy_char *src)
    {
        strncat(dest, src, size);

        dest[size - 1] = 0;
        return dest;
    }
#endif

const char *
fxmy_stristr(const char *haystack, const char * const needle)
{
    FXMY_STRISTR_BODY(char, tolower)
}

const char *
fxmy_consume_whitespace(const char *ptr)
{
    if (*ptr == 0)
        return ptr;

    while (isspace(*ptr))
        ++ptr;

    return ptr;
}

const char *
fxmy_find_end_of_string(const char *start, char string_char)
{
    FXMY_FIND_END_OF_STRING_BODY(char)
}

const char *
fxmy_next_token(const char **end_ptr, const char *str)
{
    FXMY_NEXT_TOKEN_BODY(char, fxmy_consume_whitespace, fxmy_find_end_of_string, isspace)
}

