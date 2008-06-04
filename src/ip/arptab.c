#include "../common.h"
#include "arptab.h"
#include "conf.h"
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

extern char * mac_to_str( char * buf, void * _a );

void arptab_insert( u08 iface, u32 net_addr, mac_addr phys_addr )
{
	arptab_entry * e = arptab_findslot( net_addr );
	
	if (e->net_addr != net_addr)
	{
		char phys[32];
		logf( "arp: new mapping %u.%u.%u.%u -> %s iface=%d\n", 
			net_addr & 0xff,
			net_addr >> 8 & 0xff,
			net_addr >> 16 & 0xff,
			net_addr >> 24 & 0xff,
			mac_to_str( phys, &phys_addr ), iface );
	}

	e->ttl = ARPTAB_TTL;
	e->iface = iface;
	e->net_addr = net_addr;
	e->phys_addr = phys_addr;
}

u08 arptab_query( u08 * iface, u32 net_addr, mac_addr * phys_addr )
{
	if (net_addr == get_bcastaddr() || net_addr == 0xfffffffful)
	{
		mac_addr bcast = {{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }};
		*phys_addr = bcast;
		*iface = 0xff;
		return 1;
	}

	if (!is_in_subnet( net_addr ))
	{
		logf( "arp: query for foreign address; giving default router\n" );
		net_addr = get_default_router();
	}
	
	// bogus scope so we can introduce a var
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
}

// get interface from mac address
u08 arptab_queryif( u08 * iface, mac_addr * phys_addr )
{
	u08 i;
	for( i = 0; i < ARPTAB_SIZE; i++ )
	{
		arptab_entry const * e = &arptab[i];
		if (!e->ttl) continue;
		if (mac_equal( *phys_addr, e->phys_addr ))
		{
			*iface = e->iface;
			return 1;
		}
	}

	return 0;
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