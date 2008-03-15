#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "arptab.h"
#include "../hal_debug.h"
#include "internal.h"
#include "tcp.h"

#define TCP_MAX_CONNS	40

enum tcp_state_e
{
	TCP_STATE_CLOSED = 0,
	TCP_STATE_LISTENING,
	TCP_STATE_SYN,
	TCP_STATE_ESTABLISHED,
	TCP_STATE_FINWAIT1,
	TCP_STATE_FINWAIT2,
	TCP_STATE_CLOSING,
	TCP_STATE_TIME_WAIT,
	TCP_STATE_CLOSE_WAIT,
	TCP_STATE_LAST_ACK,
};

typedef struct tcp_conn_s
{
	enum tcp_state_e state;
	u32 remotehost;
	u16 remoteport;
	u16 localport;
} tcp_conn;

static tcp_conn tcp_conns[ TCP_MAX_CONNS ];

static tcp_conn * tcp_find_listener( u16 port )
{
	tcp_conn * free = 0;
	u08 i;
	for( i = 0; i < TCP_MAX_CONNS; i++ )
	{
		tcp_conn * p = &tcp_conns[ i ];
		if (!p->state)
			free = p;
		else if (p->localport == port && !p->remotehost && !p->remoteport )
			return p;
	}
	return free;
}

static tcp_conn * tcp_find_connection( u32 remote_host, u16 remote_port, u16 port )
{
	tcp_conn * free = 0;
	u08 i;
	for( i = 0; i < TCP_MAX_CONNS; i++ )
	{
		tcp_conn * p = &tcp_conns[ i ];
		if (!p->state)
			free = p;
		else if (p->localport == port
			&& p->remotehost == remote_host
			&& p->remoteport == remote_port)
			return p;
	}
	return free;
}

#pragma pack( push, 1 )
	static struct
	{
		eth_header eth;
		ip_header ip;
		tcp_header tcp;
		u08 crap[2048];
	} out;
#pragma pack( pop )

void tcp_sendpacket( u32 dest, void* data, u16 datalen, tcp_header* inc_packet )
{
	u08 iface;

	memset( &out, 0, sizeof( out ) );

	if (!arptab_query( &iface, dest, &out.eth.dest ))
	{
		logf( "tcp: no arp cache for host, refreshing...\n" );
		send_arp_request( 0xff, dest );	// 0xff = broadcast
		return;
	}

	out.eth.src = get_macaddr();
	out.eth.ethertype = __htons( ethertype_ipv4 );
	
	out.ip.version = 0x45;
	out.ip.tos = 0;
	out.ip.length = __htons( datalen + sizeof( ip_header ) + sizeof( tcp_header ) );
	out.ip.fraginfo = 0;
	out.ip.ident = 0;
	out.ip.dest_addr = dest;
	out.ip.src_addr = get_hostaddr();
	out.ip.ttl = 128;
	out.ip.proto = IPPROTO_TCP;
	out.ip.checksum = 0;
	out.ip.checksum = ~__htons( __checksum( &out.ip, sizeof( ip_header ) ) );	
	
	out.tcp.ack_no = inc_packet->seq_no + 1;
	out.tcp.seq_no = 400;
	out.tcp.dest_port = inc_packet->src_port;
	out.tcp.src_port = inc_packet->dest_port;
	out.tcp.data_offset = 7;
	out.tcp.flags = TCP_SYN | TCP_ACK;
	out.tcp.urgent_pointer = 0;

	if( datalen )
		memcpy( out.crap, data, datalen );

	out.tcp.checksum = __checksum_ex( __pseudoheader_checksum( &out.ip ), 
		&out.tcp, datalen + sizeof( tcp_header ) );

	__send_packet( iface, (u08*)&out, sizeof( eth_header ) + sizeof( ip_header ) + sizeof( tcp_header ) + datalen );
}

u08 handle_listen_port( ip_header* p, tcp_header* t, u16 len )
{
	p; t; len;
	logf( "flags: %x\n", t->flags );

	tcp_sendpacket( p->src_addr, NULL, 0, t );
	return 1;
}

u08 handle_connection( ip_header* p, tcp_header* t, u16 len )
{
	p; t; len;
	return 1;
}

u08 tcp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	tcp_header* tcph;
	tcp_conn* conn;

	iface;
	tcph = (tcp_header*)__ip_payload( p );

	//logf( "tcp: packet from %u.%u.%u.%u:%u  for  %u.%u.%u.%u:%u\n",
	//	p->src_addr & 0xff,
	//	p->src_addr >> 8 & 0xff,
	//	p->src_addr >> 16 & 0xff,
	//	p->src_addr >> 24 & 0xff,
	//	__ntohs( tcph->src_port ),
	//	p->dest_addr & 0xff,
	//	p->dest_addr >> 8 & 0xff,
	//	p->dest_addr >> 16 & 0xff,
	//	p->dest_addr >> 24 & 0xff,
	//	__ntohs( tcph->dest_port ) );

	if( p->dest_addr != get_hostaddr() )
		return 0;

	conn = tcp_find_connection( p->src_addr, tcph->src_port, tcph->dest_port );
	if( conn && conn->state )
		return handle_connection( p, tcph, len );

	conn = tcp_find_listener( tcph->dest_port );
	if( conn && conn->state == TCP_STATE_LISTENING )
	{
		logf( "tcp: packet for socket %x (from %u.%u.%u.%u:%u)\n",
			conn - tcp_conns,
			p->src_addr & 0xff,
			p->src_addr >> 8 & 0xff,
			p->src_addr >> 16 & 0xff,
			p->src_addr >> 24 & 0xff,
			__ntohs( tcph->src_port ) );

		return handle_listen_port( p, tcph, len );
	}
	return 0;
}

tcp_sock tcp_new_listen_sock( u16 port, tcp_listen_f* new_connection_callback, tcp_recv_f* recv_callback )
{
	tcp_conn* conn;

	port = __htons( port );
	conn = tcp_find_listener( port );

	new_connection_callback; recv_callback;

	if( conn->state != TCP_STATE_CLOSED )
	{
		logf( "tcp: already listening on port %u", __ntohs( port ) );
		return INVALID_TCP_SOCK;
	}

	memset( conn, 0, sizeof( tcp_conn ) );
	conn->state = TCP_STATE_LISTENING;
	conn->localport = port;

	return (tcp_sock)(conn - tcp_conns);
}
