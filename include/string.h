/* String handling <string.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _STRING_H
#define _STRING_H _STRING_H

#include <stddef.h>  /* size_t, NULL */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory functions (implemented in core/memory.cpp) */
void * memset( void *, int, size_t );
void * memcpy( void *, const void *, size_t );
int memcmp( const void *, const void *, size_t );

/* Copying functions */
char * strcpy( char * s1, const char * s2 );
char * strncpy( char * s1, const char * s2, size_t n );

/* Concatenation functions */
char * strcat( char * s1, const char * s2 );
char * strncat( char * s1, const char * s2, size_t n );

/* Comparison functions */
int strcmp( const char * s1, const char * s2 );
int strncmp( const char * s1, const char * s2, size_t n );

/* Search functions */
void * memchr( const void * s, int c, size_t n );
char * strchr( const char * s, int c );
size_t strcspn( const char * s1, const char * s2 );
char * strpbrk( const char * s1, const char * s2 );
char * strrchr( const char * s, int c );
size_t strspn( const char * s1, const char * s2 );
char * strstr( const char * s1, const char * s2 );
char * strtok( char * s1, const char * s2 );

/* Miscellaneous functions */
size_t strlen( const char * s );

/* Non-standard kernel extensions */
uint32_t HexStrToInt( const char * str );

#ifdef __cplusplus
}
#endif

#endif
