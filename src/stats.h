#ifndef STATS_H
#define STATS_H

typedef struct counter_t
{
	char const * name;
	u64 count;
} counter_t;

void * stats_new_counter (char const * counter_name);
void stats_inc_counter (void * p_counter);
u64 stats_get_counter_count (void * p_counter);
char const * stats_get_counter_name (void * p_counter);
void * stats_get_next_counter ( void * p_counter );

#endif