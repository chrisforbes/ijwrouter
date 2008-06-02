#include "common.h"
#include "hal_debug.h"
#include "hal_time.h"

#define _USE_32BIT_TIME_T
#pragma warning( disable: 4133 )
#pragma warning( disable: 4244 )
#include <time.h>
#pragma warning( default: 4244 )
#pragma warning( default: 4133 )

static u08 rollover_day	= 1;

u32 get_start_of_period(void)
{
	u32 current = get_time();
	struct tm * t = gmtime( (time_t const *)&current );

	if (t->tm_mday <= rollover_day)
	{
		if (t->tm_mon)
			t->tm_mon--;
		else
		{
			t->tm_year--;
			t->tm_mon = 11;
		}
	}

	t->tm_hour = 0;
	t->tm_min = 0;
	t->tm_sec = 0;
	t->tm_mday = rollover_day;
	return (u32)mktime( t );
}

u32 get_end_of_period(void)
{
	u32 current = get_time();
	struct tm * t = gmtime( (time_t const *)&current );

	if (t->tm_mday > rollover_day)
	{
		if (t->tm_mon < 11)
			t->tm_mon++;
		else
		{
			t->tm_year++;
			t->tm_mon = 0;
		}
	}

	t->tm_hour = 0;
	t->tm_min = 0;
	t->tm_sec = 0;
	t->tm_mday = rollover_day;
	return (u32)mktime( t );
}