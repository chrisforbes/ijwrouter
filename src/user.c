#include "common.h"
#include "user.h"
#include "ip/conf.h"
#include "hal_debug.h"
#include "ip/arptab.h"
#include <assert.h>
#include "table.h"

#define MAX_USERS	128
#define MAX_MAPPINGS	512	// most people have <= 4 network cards

char * mac_to_str( char * buf, void * _a );	// extern 

typedef struct mac_mapping_t
{
	mac_addr eth_addr;
	user_t * user;
} mac_mapping_t;

DEFINE_TABLE( mac_mapping_t, mappings, MAX_MAPPINGS );
DEFINE_TABLE( user_t, users, MAX_USERS );

u08 pred_is_mac_equal( mac_mapping_t * mapping, mac_addr * addr )
	{ return mac_equal( mapping->eth_addr, *addr ); }

void remap_user( user_t * from, user_t * to ) 
{
	FOREACH( mac_mapping_t, mappings, m )
		if (m->user == from)
			m->user = to;
}

user_t * get_user( mac_addr eth_addr )
{
	mac_mapping_t * m = FIND_TABLE_ENTRY( mac_mapping_t, mappings, pred_is_mac_equal, &eth_addr );
	if (!m)
	{
		user_t * u = ALLOC_TABLE_ENTRY( user_t, users );
		if (!u) return 0;

		mac_to_str( u->name, &eth_addr );
		u->credit = 0;
		u->quota = 0;
		
		m = ALLOC_TABLE_ENTRY( mac_mapping_t, mappings ); 
		m->eth_addr = eth_addr;

		m->user = u;
	}

	assert( FIND_TABLE_ENTRY( mac_mapping_t, mappings, pred_is_mac_equal, &eth_addr ) == m );
	return m->user;
}

user_t * get_user_by_ip( u32 addr )
{
	mac_addr eth_addr;
	
	if (!is_in_subnet( addr ))
	{
		logf( "user: ip not in subnet" );
		return 0;	// horribly misconfigured
	}

	if (!arptab_query( 0, addr, &eth_addr ))
	{
		logf( "user: no arp cache for ip" );
		return 0;	// cant translate to mac address
	}

	return get_user( eth_addr );
}

extern u32 __stdcall inet_addr (char const * cp);

user_t * get_next_user( user_t * u )
{
	return FIND_TABLE_ENTRY_FROM( user_t, users, __always, 0, u );
}
