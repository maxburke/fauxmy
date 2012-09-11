#ifndef FXMY_STRING_H
#define FXMY_STRING_H

#include <string.h>

#include "fxmy_string_type.h"

#ifdef _MSC_VER
    #define fxmy_strstr wcsstr
    #define fxmy_strlen wcslen
    fxmy_char *fxmy_strfromchar(fxmy_char *dest, const char *src, size_t num_chars);
#else
    #define fxmy_strstr strstr
    #define fxmy_strlen strlen
    #define fxmy_strfromchar memcpy
#endif

#endif