#include "common.h"
#include "user.h"
#include "ip/conf.h"
#include "hal_debug.h"
#include "hal_time.h"
#include "ip/arptab.h"
#include <assert.h>
#include "table.h"
#include "billing.h"

#include <stdio.h>
#include <windows.h>

#define MAX_USERS	128
#define MAX_MAPPINGS	512	// most people have <= 4 network cards

char * mac_to_str( char * buf, void * _a );	// extern 
extern u32 __stdcall inet_addr (char const * cp);

DEFINE_TABLE( mac_mapping_t, mappings, MAX_MAPPINGS );
DEFINE_TABLE( user_t, users, MAX_USERS );

u08 pred_is_mac_equal( mac_mapping_t * mapping, mac_addr * addr )
	{ return mac_equal( mapping->eth_addr, *addr ); }

user_t * get_user( mac_addr eth_addr )
{
	mac_mapping_t * m = FIND_TABLE_ENTRY( mac_mapping_t, mappings, pred_is_mac_equal, &eth_addr );
	if (!m)
	{
		user_t * u = ALLOC_TABLE_ENTRY( user_t, users );
		if (!u) return 0;

		mac_to_str( u->name, &eth_addr );
		u->credit = 0;
		u->references = 1;
		u->flags = USER_NEW;
		
		m = ALLOC_TABLE_ENTRY( mac_mapping_t, mappings ); 
		m->eth_addr = eth_addr;
		m->user = u;
	}

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

user_t * get_user_by_name( char const * name )
{
	FOREACH( user_t, users, x )
		if (strcmp(x->name, name) == 0)
			return x;

	return 0;
}

user_t * get_next_user( user_t * u )
{
	return FIND_TABLE_ENTRY_FROM( user_t, users, __always, 0, u );
}

mac_mapping_t * get_next_mac ( mac_mapping_t * m )
{
	return FIND_TABLE_ENTRY_FROM( mac_mapping_t, mappings, __always, 0, m );
}

void remap_user( user_t * from, user_t * to ) 
{
	FOREACH( mac_mapping_t, mappings, m )
		if (m->user == from)
			m->user = to;
}

void merge_users( user_t * from, user_t * to )
{
	if (from == to)
	{
		logf("user: tried to merge self\n");
		return;
	}

	logf("user: merged %s into %s\n", from->name, to->name);
	to->credit += from->credit;
	to->references += from->references;
	to->last_credit += from->last_credit;
	remap_user(from, to);
	FREE_TABLE_ENTRY( user_t, users, from, remap_user );
}


// --- NOT PORTABLE - WIN32 ONLY -------------------------

typedef struct persist_header_t
{
	u32 num_users;
	u32 num_mappings;
	void * user_base;
	void * mapping_base;
	u08 rollover_day;	// for billing period
} persist_header_t;

void save_users( void )
{
#pragma warning( disable: 4204 )
	persist_header_t h = 
	{ 
		NUM_ROWS( user_t, users ), 
		NUM_ROWS( mac_mapping_t, mappings ),
		users,
		mappings,
		get_rollover_day()
	};
#pragma warning( default: 4204 )

	FILE * f = fopen( "../accounts.temp.dat", "wb" );
	if (!f)
	{
		logf( "failed persisting account db: cannot open ../accounts.temp.dat\n" );
		return;
	}

	fwrite( &h, sizeof( persist_header_t ), 1, f );
	fwrite( users, sizeof( user_t ), h.num_users, f );
	fwrite( mappings, sizeof( mac_mapping_t ), h.num_mappings, f );

	fclose(f);

	// finally, atomic overwrite
	CopyFileA( "../accounts.temp.dat", "../accounts.dat", 0 );
	DeleteFileA( "../accounts.temp.dat" );
}

void restore_users( void )
{
	FILE * f = fopen( "../accounts.dat", "rb" );
	persist_header_t h;

	if (!f)
	{
		logf( "failed restoring account db: cannot open ../accounts.dat\n" );
		return;
	}

	fread( &h, sizeof( persist_header_t ), 1, f );

	set_rollover_day( h.rollover_day );

	fread( users, sizeof( user_t ), h.num_users, f );
	fread( mappings, sizeof( mac_mapping_t ), h.num_mappings, f );

	fclose( f );

	// patch the table sizes
	NUM_ROWS( user_t, users ) = h.num_users;
	NUM_ROWS( mac_mapping_t, mappings ) = h.num_mappings;

	// patch the pointers in the mappings table (address space layout is NOT PORTABLE)
	{
		FOREACH( mac_mapping_t, mappings, x )
			if (x->user)
				x->user = (user_t *)( ((u08*) x->user) - 
				(u08*)h.mapping_base + (u08 *)mappings );
	}
}

#define SAVE_INTERVAL		300000	// 5 minutes

void do_periodic_save( void )
{
	static u32 last_save = 0;
	static u08 is_billing_inited = 0;
	static u32 last_period_end = 0;

	if (ticks() - last_save >= SAVE_INTERVAL)
	{		
		u32 period_end = get_end_of_period();

		if (!is_billing_inited)
		{
			last_period_end = period_end;
			is_billing_inited = 1;
		}

		if (get_time() > last_period_end)	// todo: fix corner cases
		{
			FOREACH( user_t, users, x )
			{
				x->last_credit = x->credit;
				x->credit = 0;
			}

			logf("user: --- started new billing period ---\n");
			last_period_end = period_end;
		}

		save_users();
		last_save = ticks();
	}
}