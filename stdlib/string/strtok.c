/* strtok( char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

char * strtok( char * s1, const char * s2 )
{
    static char * next = NULL;

    if ( s1 != NULL )
    {
        next = s1;
    }

    if ( next == NULL )
    {
        return NULL;
    }

    /* skip leading delimiters */
    while ( *next )
    {
        const char * p = s2;
        while ( *p )
        {
            if ( *next == *p )
            {
                break;
            }
            ++p;
        }
        if ( *p == '\0' )
        {
            break;
        }
        ++next;
    }

    if ( *next == '\0' )
    {
        next = NULL;
        return NULL;
    }

    char * token = next;

    /* find next delimiter */
    while ( *next )
    {
        const char * p = s2;
        while ( *p )
        {
            if ( *next == *p )
            {
                *next++ = '\0';
                return token;
            }
            ++p;
        }
        ++next;
    }

    next = NULL;
    return token;
}
