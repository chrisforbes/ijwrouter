#include "../common.h"
#include "../hal_time.h"

#define _USE_32BIT_TIME_T

#include <windows.h>
#pragma warning( disable: 4133 )
#pragma warning( disable: 4244 )
#include <time.h>

u32 ticks(void) { return GetTickCount(); }

static u32 time_offset = 0;

u32 get_time(void)
{
	return (u32)time(0) + time_offset;
}

void set_time( u32 time )
{
	time_offset = time - get_time();
	// check that this (at least sortof) works:
	logf( "time set to: localosc%+d = %s", 
		time_offset, ctime( (time_t const *) &time ));
}