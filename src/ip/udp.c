#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "arp.h"
#include "arptab.h"
#include "udp.h"
#include "conf.h"
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

u08 udp_receive_packet( ip_header * p, u16 len )
{
	udp_header * udp = ( udp_header * ) __ip_payload( p );
	udp_conn * conn;
	udp_sock sock;
	u16 port = __ntohs( udp->dest_port );

	len;

	// todo: verify udp checksum

	conn = udp_find_conn_by_port( port );
	if (!conn || !conn->handler)
	{
//		log_printf( "udp: no conn for port=%u\n", port );
		return 0;
	}

	sock = (udp_sock)(conn - udp_conns);

//	log_printf( "udp: delivering datagram for port=%u to sock %u\n", port, sock );

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
		log_printf( "udp: too many sockets.\n" );
		return INVALID_UDP_SOCK;
	}

	if (conn->handler)
	{
		log_printf( "udp: port already bound to another socket.\n" );
		return INVALID_UDP_SOCK;
	}

	conn->handler = handler;
	conn->ctx = ctx;
	conn->port = port;

	log_printf( "udp: bound port %u to socket %u\n", port, (conn - udp_conns) );

	return (udp_sock)(conn - udp_conns);
}

#include "../pack1.h"
	static struct
	{
		eth_header eth;
		ip_header ip;
		udp_header udp;
		u08 crap[2048];
	} PACKED_STRUCT out;
#include "../packdefault.h"

void udp_send( udp_sock sock, u32 to_ip, u16 to_port, u08 const * data, u16 len )
{
	udp_conn * conn = &udp_conns[sock];
	u08 iface;

	if (!conn->handler)
	{
		log_printf( "udp: not_sock in send\n" );
		return;
	}

	memset( &out, 0, sizeof( out ) );

	if ( !arp_make_eth_header( &out.eth, to_ip, &iface ) )
		return;

	out.eth.src = get_macaddr();
	out.eth.ethertype = __htons( ethertype_ipv4 );
	
	__ip_make_header( &out.ip, IPPROTO_UDP, 0,
		len + sizeof( ip_header ) + sizeof( udp_header ),
		to_ip );

	out.udp.src_port = __htons(conn->port);
	out.udp.dest_port = __htons(to_port);
	out.udp.length = __htons( len + sizeof( udp_header ) );

	if ( len )
		memcpy( out.crap, data, len );

	out.udp.checksum = ~__htons(__checksum_ex( 
		__pseudoheader_checksum( &out.ip ),
		&out.udp, len + sizeof( udp_header ) ));

	__send_packet( iface, (u08 const *) &out, sizeof( eth_header ) + sizeof( ip_header ) + sizeof( udp_header ) + len );
}

void udp_close( udp_sock sock )
{
	udp_conn * conn = &udp_conns[sock];

	if ( !conn )
	{
		log_printf( "udp: not_sock in close\n" );
		return;
	}

	conn->handler = 0;
	conn->ctx = 0;
	conn->port = 0;
}
