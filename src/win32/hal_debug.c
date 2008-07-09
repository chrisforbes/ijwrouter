// console implementation of hal_debug

#include "../common.h"
#include "../hal_debug.h"

#include <stdarg.h>
#include <stdio.h>

void log_printf( char const * str, ... )
{
	va_list vl;

	va_start( vl, str );
	vfprintf( stdout, str, vl );
	va_end( vl );
}