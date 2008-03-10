#include "../common.h"
#include "arptab.h"
#include "../hal_debug.h"

#define ARPTAB_SIZE		64
#define ARPTAB_TTL		1000

typedef struct arptab_entry
{
	u32 ttl;
	u08 iface;
	u32 net_addr;
	mac_addr phys_addr;
} arptab_entry;

static arptab_entry arptab[ ARPTAB_SIZE ];

static arptab_entry * arptab_findslot( u32 net_addr )
{
	arptab_entry * oldest = 0;

	u08 i;
	for( i = 0; i < ARPTAB_SIZE; i++ )
	{
		arptab_entry * e = &arptab[i];
		if (e->net_addr == net_addr && e->ttl)
			return e;

		if (!oldest || e->ttl <= oldest->ttl)
			oldest = e;
	}

	return oldest;
}

void arptab_insert( u08 iface, u32 net_addr, mac_addr phys_addr )
{
	arptab_entry * e = arptab_findslot( net_addr );
	e->ttl = ARPTAB_TTL;
	e->iface = iface;
	e->net_addr = net_addr;
	e->phys_addr = phys_addr;
}

u08 arptab_query( u08 * iface, u32 net_addr, mac_addr * phys_addr )
{
	arptab_entry * e = arptab_findslot( net_addr );

	if (!e->ttl || e->net_addr != net_addr)
		return 0;

	if (iface)	
		*iface = e->iface;
	if (phys_addr) 
		*phys_addr = e->phys_addr;

	return 1;
}

void arptab_tick( void )
{
	u08 i;
	for( i = 0; i < ARPTAB_SIZE; i++ )
	{
		if (arptab[i].ttl)
			--arptab[i].ttl;
	}
}