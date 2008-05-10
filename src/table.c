// table maintenance helpers for ijw-router

#include "common.h"
#include "table.h"

void * __find_table_entry( void * begin, void * end, u32 entry_size, u32 * num_in_use, generic_predicate * p, void * parg, void * from )
{
	u32 n = *num_in_use;
	u08 * i = (u08 *)begin;

	if (from)
		while( i <= (u08*)from && n-- )
			i += entry_size;

	while( i < (u08*)end && n-- )
	{
		if (p( (void *)i, parg ))
			return i;
		i += entry_size;
	}

	return 0;
}

u08 __free_table_entry( void * begin, void * end, u32 entry_size, u32 * num_in_use, void * entry, generic_remap * remap )
{
	void * last;

	if (entry < begin || entry >= end || !*num_in_use)
		return 0;	// out of range

	(*num_in_use)--;

	last = (u08*)begin + (*num_in_use * entry_size);
	if (last != entry)
	{
		memcpy( entry, last, entry_size );
		remap( last, entry );
	}

	return 1;	// success
}

void * __alloc_table_entry( void * begin, u32* num_in_use, u32 entry_size )		// handle 'out of space' hax
{
	void * ret = (u08 *)begin + (*num_in_use * entry_size );
	memset( ret, 0, entry_size );
	(*num_in_use)++;
	return ret;
}

u08 __always(void* a,void* b) { a;b; return 1; }
u08 __never(void* a,void* b) { a;b; return 0; }