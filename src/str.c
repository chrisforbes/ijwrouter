#include "common.h"
#include "str.h"

#include "hal_debug.h"

u08 decode_hex( char c )
{
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= '0' && c <= '9')
		return c - '0';
	logf("char %c is not a valid hex digit\n", c);
	return 0;
}

void uri_decode( char * dest, u32 len, char const * src )
{
	u08 c, s = 0;
	while (0 != (c = *src++) && len--)
	{
		switch (s)
		{
		case 0:
			if (c == '%') s = 1;
			else if (c == '+') *dest++ = ' '; //hack to get around form encoding of space characters
			else *dest++ = c;
			break;
		case 1:
			*dest = decode_hex( c ) << 4;
			s = 2;
			break;
		case 2:
			*dest++ |= decode_hex( c );
			s = 0;
			break;
		}
	}
	*dest = 0;
}
