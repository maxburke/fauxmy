#ifndef FXMY_STRING_TYPE_H
#define FXMY_STRING_TYPE_H

/*
 * This file defines the Fauxmy character type. On Windows this is a wide
 * character as MSSQL performs some funky character set conversion unless
 * you use the wide-char API. 
 *
 * Specifically this file defines the FXMYCHAR type, which is the char
 * type used internally, and the macro C(x) which creates a C-string
 * or character literal with that type.
 */

#ifdef _MSC_VER
    typedef wchar_t fxmy_char;
    #define C(x) L ## x
#else
    typedef char fxmy_char;
    #define C(x) x
#endif
    
#endif