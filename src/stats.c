#include "common.h"
#include "str.h"
#include "stats.h"
#include "table.h"

#define MAX_COUNTERS 16

DEFINE_TABLE(counter_t, counters, MAX_COUNTERS);

u08 is_counter_name_equal( counter_t * c, char const * name )
{
	return (c->name == name);
}

void * stats_new_counter (char const * counter_name)
{
	counter_t * c = FIND_TABLE_ENTRY(counter_t, counters, is_counter_name_equal, counter_name);
	
	if (!c)
	{
		c = ALLOC_TABLE_ENTRY(counter_t, counters);
		c->name = counter_name;
		c->count = 0;
	}

	return c;
}

void stats_inc_counter (void * p_counter, u64 amount)
{
	counter_t * c = (counter_t *)p_counter;

	c->count += amount;
}

u64 stats_get_counter_count (void * p_counter)
{
	counter_t * c = (counter_t *)p_counter;
	return c->count;
}

char const * stats_get_counter_name (void * p_counter)
{
	counter_t * c = (counter_t *)p_counter;
	return c->name;
}

void * stats_get_next_counter ( void * p_counter )
{
	return FIND_TABLE_ENTRY_FROM(counter_t, counters, __always, 0, p_counter);
}
