// table maintenance helpers for ijw-router

#define DEFINE_TABLE( T, name, size )\
	T name[size];\
	u32 __ ## name ## _count = 0

typedef u08 generic_predicate( void * value, void * ctx );
typedef void generic_remap( void * from, void * to );

void * __find_table_entry( void * begin, void * end, u32 entry_size, u32 * num_in_use, generic_predicate * p, void * parg, void * from );
u08 __free_table_entry( void * begin, void * end, u32 entry_size, u32 * num_in_use, void * entry, generic_remap * remap );
void * __alloc_table_entry( void * begin, u32* num_in_use, u32 entry_size );

u08 __always( void *, void * );		// standard predicates
u08 __never( void *, void * );

#define FIND_TABLE_ENTRY( T, name, predicate, parg )\
	(T*)__find_table_entry( name, (u08*)name + sizeof(name), \
	sizeof(T), &__ ## name ## _count, (generic_predicate *) predicate, (void *)parg, &name[-1] )

#define FIND_TABLE_ENTRY_FROM( T, name, predicate, parg, from )\
	(T*)__find_table_entry( name, (u08*)name + sizeof(name), \
	sizeof(T), &__ ## name ## _count, (generic_predicate *) predicate, (void *)parg, (void*)from )

#define ALLOC_TABLE_ENTRY( T, name )\
	(T*)__alloc_table_entry( name, &__ ## name ## _count, sizeof(T) )

#define FREE_TABLE_ENTRY( T, name, entry, remap )\
	__free_table_entry( name, sizeof(T), (u08*)name + sizeof(name), &__ ## name ## _count, entry, (generic_remap *)remap )

#define FOREACH( T, name, var )\
	T* var = 0; while( 0 != (var = FIND_TABLE_ENTRY_FROM( T, name, __always, 0, var )))
