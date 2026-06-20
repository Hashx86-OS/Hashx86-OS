/* HexStrToInt( const char * )

   Public domain.
*/

#include <stdint.h>

uint32_t HexStrToInt( const char * str )
{
    uint32_t result = 0;

    while ( *str == ' ' || *str == '\t' )
    {
        ++str;
    }

    if ( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ) )
    {
        str += 2;
    }

    while ( *str )
    {
        char c = *str;
        uint32_t val;

        if ( c >= '0' && c <= '9' )
        {
            val = c - '0';
        }
        else if ( c >= 'a' && c <= 'f' )
        {
            val = c - 'a' + 10;
        }
        else if ( c >= 'A' && c <= 'F' )
        {
            val = c - 'A' + 10;
        }
        else
        {
            break;
        }

        result = ( result << 4 ) | val;
        ++str;
    }

    return result;
}
