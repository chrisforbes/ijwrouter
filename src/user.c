#include "common.h"
#include "user.h"
#include "ip/conf.h"
#include "hal_debug.h"
#include "ip/arptab.h"
#include <assert.h>

#define MAX_USERS	128
#define MAX_MAPPINGS	512	// most people have <= 4 network cards

static user usertab[ MAX_USERS ] = {0};
static u08 _n = 0;

char * mac_to_str( char * buf, void * _a );	// extern 

typedef struct mac_mapping_t
{
	mac_addr eth_addr;
	u08 uid;
} mac_mapping_t;

static mac_mapping_t mappings[ MAX_MAPPINGS ] = {0};
static int _last_mapping = 0;

static mac_mapping_t * get_mac_mapping( mac_addr addr, u08 create_if_missing )
{
	int i;
	for( i = 0; i < MAX_MAPPINGS; i++ )
	{
		mac_mapping_t * m = &mappings[ i ];

		if (mac_equal( addr, m->eth_addr ) && m->uid)
			return m;

		if (!m->uid && create_if_missing)
		{
			m->eth_addr = addr;
			_last_mapping = i;
			return m;
		}
	}

	return 0;
}

static void remove_mapping( mac_mapping_t * m )
{
	mac_mapping_t * last = &mappings[ _last_mapping ];
	assert( m );
	
	if (last != m)
	{
		*m = *last;
		--_last_mapping;
	}

	memset( last, 0, sizeof(mac_mapping_t) );
}

static user * get_free_user(void)
{
	u08 start_point = _n;
	
	do
	{
		user * u = &usertab[ _n ];
		if ( *u->name )
		{
			++_n;
			_n %= MAX_USERS;
			continue;
		}

		return u;
	}
	while( _n != start_point );

	logf( "user: no free entries in table" );
	return 0;
}

static u08 is_in_subnet( u32 ip )
{
	u32 mask = get_netmask();
	return (get_hostaddr() & mask) == (ip & mask);
}

user * get_user( mac_addr eth_addr )
{
	mac_mapping_t * m = get_mac_mapping( eth_addr, 1 );

	if (!m->uid)
	{
		user * u = get_free_user();
		if (!u) return 0;

		m->uid = (u08)( 1 + ( u - usertab ) );
		mac_to_str( u->name, &eth_addr );
		u->credit = 0;
		u->quota = 0;
		return u;
	}
	else
		return &usertab[ m->uid - 1 ];
}

user * get_user_by_ip( u32 addr )
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

user * get_next_user( user * u )
{
	u08 i;
	if (!u)
		return *usertab[0].name ? &usertab[0] : 0;

	i = (u08)( u - usertab );
	if (i >= MAX_USERS)
		return 0;
	
	return (*(++u)->name) ? u : 0;
}

void enumerate_users( user ** u, u32 * num_users )
{
	*u = usertab;
	*num_users = 0;
}