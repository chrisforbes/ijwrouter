#pragma once

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

#define MAKE_STRING( x )	__make_string(x, sizeof(x) - 1)
