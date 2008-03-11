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

u08 udp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	udp_header * udp = ( udp_header * ) __ip_payload( p );
	udp_conn * conn;
	udp_sock sock;
	u16 port = __ntohs( udp->dest_port );

	iface; len;

	logf( "udp: got packet\n" );

	// todo: verify udp checksum

	conn = udp_find_conn_by_port( port );
	if (!conn || !conn->handler)
	{
		logf( "udp: no conn for port=%u\n", port );
		return 0;
	}

	sock = (udp_sock)(conn - udp_conns);

	logf( "udp: delivering datagram for port=%u\n to sock %u\n", port, sock );

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

#pragma pack( push, 1 )
	static struct
	{
		eth_header eth;
		ip_header ip;
		udp_header udp;
		u08 crap[2048];
	} out;
#pragma pack( pop )

#pragma pack( push, 1 )
	static struct		// ip pseudo-header for udp checksum
	{
		u32 src;
		u32 dest;
		u08 sbz;
		u08 proto;
		u16 len;
	} ipph;

void udp_send( udp_sock sock, u32 to_ip, u16 to_port, u08 const * data, u16 len )
{
	udp_conn * conn = &udp_conns[sock];
	u08 iface;

	if (!conn->handler)
	{
		logf( "udp: not_sock in send\n" );
		return;
	}

	memset( &out, 0, sizeof( out ) );

	if (!arptab_query( &iface, to_ip, &out.eth.dest ))
	{
		logf( "udp: no arp cache for host, refreshing...\n" );
		send_arp_request( 0xff, to_ip );	// 0xff = broadcast
		return;
	}

	out.eth.src = get_macaddr();
	out.eth.ethertype = __htons( ethertype_ipv4 );
	
	out.ip.version = 0x45;
	out.ip.tos = 0;
	out.ip.length = __htons( len + sizeof( ip_header ) + sizeof( udp_header ) );
	out.ip.fraginfo = 0;
	out.ip.ident = 0;
	out.ip.dest_addr = to_ip;
	out.ip.src_addr = get_hostaddr();
	out.ip.ttl = 128;
	out.ip.proto = IPPROTO_UDP;
	out.ip.checksum = 0;
	out.ip.checksum = ~__htons( __checksum( &out.ip, sizeof( ip_header ) ) );	

	out.udp.src_port = __htons(conn->port);
	out.udp.dest_port = __htons(to_port);
	out.udp.length = __htons( len + sizeof( udp_header ) );

	memcpy( out.crap, data, len );

	ipph.src = out.ip.src_addr;
	ipph.dest = out.ip.dest_addr;
	ipph.proto = out.ip.proto;
	ipph.sbz = 0;
	ipph.len = out.udp.length;

	out.udp.checksum = ~__htons(__checksum_ex( 
		__checksum( &ipph, sizeof(ipph) ), 
		&out.udp, len + sizeof( udp_header ) ));

	__send_packet( iface, (u08 const *) &out, sizeof( eth_header ) + sizeof( ip_header ) + sizeof( udp_header ) + len );
}