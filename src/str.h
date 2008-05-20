#ifndef STR_H
#define STR_H

#pragma warning(disable:4204)

typedef struct str_t
{
	char * str;
	u32 len;
} str_t;

__inline str_t __make_string( char * x, u32 len ) { str_t s = { x, len }; return s; }

__inline void grow_string( str_t * s, u32 extra )
{
	s->len += extra;
	s->str = realloc( s->str, s->len );
}

__inline void append_string( str_t * dest, str_t const * src )
{
	u32 len = dest->len;
	grow_string( dest, src->len );
	memcpy( dest->str + len, src->str, src->len );
}

#define MAKE_STRING( x )	__make_string(x, sizeof(x) - 1)

u08 decode_hex( char c );
void uri_decode( char * dest, u32 len, char const * src );

	// zero-terminating memcpy
static void __inline __memcpyz( char * dest, char const * src, u32 len )
{
	memcpy( dest, src, len );
	dest[len] = 0;
}

__inline char * format_amount( char * buf, u64 x )
{
	static char const * units[] = { "KB", "MB", "GB", "TB" };
	char const ** u = units;

	while( x > (1 << 20) * 9 / 10 )
	{
		x >>= 10;
		u++;
	}

	sprintf( buf, "%1.2f %s", x / 1024.0f, *u );
	return buf;
}

#endif
