#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "arp.h"
#include "udp.h"
#include "../hal_debug.h"
#include "internal.h"

#define UDP_MAX_CONNS	40

typedef struct udp_conn
{
	void * ctx;
	udp_event_f * handler;
	u16 port;
} udp_conn;

static udp_conn udp_conns[ UDP_MAX_CONNS ];

static udp_conn * udp_find_conn_by_port( u16 port )
{
	udp_conn * free = 0;
	u08 i;
	for( i = 0; i < UDP_MAX_CONNS; i++ )
	{
		udp_conn * p = &udp_conns[ i ];
		if (!p->handler)
			free = p;
		else if (p->port == port)
			return p;
	}

	return free;
}

u08 udp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	udp_header * udp = ( udp_header * ) __ip_payload( p );
	udp_conn * conn;
	udp_sock sock;

	iface; len;

	// todo: verify udp checksum

	conn = udp_find_conn_by_port( __ntohs( udp->dest_port ) );
	if (!conn || !conn->handler)
	{
		logf( "udp: no conn for port=%u\n", __ntohs( udp->dest_port ) );
		return 0;
	}

	sock = (udp_sock)(conn - udp_conns);

	logf( "udp: delivering datagram for port=%u\n to sock %u\n", __ntohs( udp->dest_port ), sock );

	conn->handler( sock, 
		UDP_EVENT_PACKET, 
		p->src_addr, 
		__ntohs( udp->src_port ), 
		(u08 const *)( udp + 1 ), 
		__ip_payload_length( p ) - sizeof( udp_header ) );

	return 0;
}

void * udp_get_ctx( udp_sock sock )
{
	udp_conn * p;

	if ( sock >= UDP_MAX_CONNS )
		return 0;

	p = &udp_conns[ sock ];
	return p->handler ? p->ctx : 0;
}

static u16 dynport = 49152;

static u16 make_dynamic_port(void) { return dynport++; }	// todo: fix overflow at 64k

udp_sock udp_new_sock( u16 port, void * ctx, udp_event_f * handler )
{
	udp_conn * conn = udp_find_conn_by_port( port ? port : (port = make_dynamic_port()) );
	
	if (!conn)
	{
		logf( "udp: too many sockets.\n" );
		return INVALID_UDP_SOCK;
	}

	if (conn->handler)
	{
		logf( "udp: port already bound to another socket.\n" );
		return INVALID_UDP_SOCK;
	}

	conn->handler = handler;
	conn->ctx = ctx;
	conn->port = port;

	logf( "udp: bound port %u to socket %u\n", port, (conn - udp_conns) );

	return (udp_sock)(conn - udp_conns);
}