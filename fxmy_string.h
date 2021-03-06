#ifndef FXMY_STRING_H
#define FXMY_STRING_H

#pragma once

#include <string.h>

#include "fxmy_string_type.h"

#ifdef _MSC_VER
    #pragma warning(push, 0)
    #include <wchar.h>
    #pragma warning(pop)

    #define fxmy_fstrstr wcsstr
    #define fxmy_fstrlen wcslen
    #define fxmy_fstrncpy wcsncpy
    #define fxmy_fsnprintf _snwprintf
    #define fxmy_fstrncmp wcsncmp

    fxmy_char *
    fxmy_fstrfromchar(fxmy_char *dest, const char *src, size_t num_chars);

    size_t
    fxmy_fstrlenfromchar(const char *src, size_t size);

    const fxmy_char *
    fxmy_fstristr(const fxmy_char *haystack, const fxmy_char * const needle);

    const fxmy_char *
    fxmy_fnext_token(const fxmy_char **end_ptr, const fxmy_char *str);

    int
    fxmy_fstrnicmp(const fxmy_char *a, const fxmy_char *b, size_t length);
#else
    #define fxmy_fstrstr strstr
    #define fxmy_fstrlen strlen
    #define fxmy_fstrcpy strncpy

    /*
     * This is unsafe as strfromchar should ensure that the destination is 
     * zero-terminated.
     */
    /* #define fxmy_strfromchar memcpy */

    /*
     * Similar to the above, strncatfromchar should ensure the destination is
     * zero-terminated.
     */
    /* #define fxmy_strncatfromchar strncat */

    #define fxmy_fsnprintf snprintf
    #define fxmy_fstristr fxmy_stristr
    #define fxmy_fnext_token fxmy_next_token

    size_t
    fxmy_strlenfromchar(const char *src, size_t size);
#endif

fxmy_char *
fxmy_fstrncat(fxmy_char *dest, size_t size, const fxmy_char *src);

const char *
fxmy_stristr(const char *haystack, const char * const needle);

const char *
fxmy_consume_whitespace(const char *ptr);

const char *
fxmy_find_end_of_string(const char *start, char string_char);

const char *
fxmy_next_token(const char **end_ptr, const char *str);

#endif