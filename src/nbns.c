#include <string.h>
#include "common.h"
#include "hal_debug.h"
#include "hal_time.h"

#include "ip/rfc.h"
#include "ip/udp.h"
#include "ip/conf.h"

#define NBNS_PORT	137

static udp_sock sock;

#pragma pack( push, 1 )

typedef struct nbns_query
{
	u16 transaction;
	u16 flags;
	u16 questions;
	u16 answers;
	u16 authority;
	u16 additional;
} nbns_query;

typedef struct nbns_record
{
	u08 foo;
	u08 name[32];
	u08 bs;
	u16 type;
	u16 clazz;
} nbns_record;

typedef struct nbns_response
{
	u32 ttl;
	u16 length;	// 6
	u16 flags;
	u32 ip_addr;
} nbns_response;

#pragma pack( pop )

static void unpack_name( u08* out, u08 const * in )
{
	int i = 0;
	for(  ; i < 16 ; i++ )
	{
		out[i] = ((in[i*2] - 'A') << 4) + in[i*2+1] - 'A';
		if( out[i] == ' ' )
			out[i] = 0 ;
	}
	out[16] = 0;
}

static void send_response( u32 from_ip, u16 from_port, nbns_query const * q, nbns_record const * r )
{
#pragma pack( push, 1 )
	struct
	{
		nbns_query q;
		nbns_record r;
		nbns_response rs;
	} hax = { { 0, 0x85, 0, 0x100, 0, 0 } } ;
#pragma pack( pop )
	hax.q.transaction = q->transaction;
	hax.r = *r;
	
	hax.rs.ttl = 0xe0930400;	// a few days
	hax.rs.length = __htons(6);
	hax.rs.flags = 0;	// todo: are we actually a B node?
	hax.rs.ip_addr = get_hostaddr();

	udp_send( sock, from_ip, from_port, (u08 const *)&hax, sizeof( hax ) );
}

static void nbns_event( udp_sock sock, udp_event_e evt, 
	u32 from_ip, u16 from_port, u08 const * data, u16 len )
{
	nbns_query const * q;

	if (evt != UDP_EVENT_PACKET) return;
	sock; evt; from_ip; from_port; data; len;
	
	// todo: interpret this
	q = (nbns_query const *) data;
	
	if ( q->flags == 0x1001 && q->questions )	// query			todo: fix this for other cases
	{
		nbns_record const * r = (nbns_record const *) (q+1);
		u16 questions = __ntohs(q->questions);

		logf( "nbns: got query\n" );

		while( questions-- )
		{
			u08 name[17];
			unpack_name( name, r->name );
			logf( "nbns: got name request for %s\n", name );
			if( strcmp( "IJW-ROUTER", (char const *)name ) == 0 )
				send_response( from_ip, from_port, q, r );
			++r;
		}
	}
}

void nbns_init( void )
{
	sock = udp_new_sock( NBNS_PORT, 0, nbns_event );
	if (sock == INVALID_UDP_SOCK)
	{
		logf( "nbns: unable to bind socket\n" );
		return;
	}

	logf( "nbns: started successfully\n" );
}

void nbns_process( void )
{
}