#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "arptab.h"
#include "../hal_debug.h"
#include "internal.h"
#include "tcp.h"

#include <assert.h>

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

typedef struct tcp_buf
{
	u08 const * data;
	u32 len;
	u32 ofs;
	struct tcp_buf * next;
} tcp_buf;

typedef struct tcp_conn_s
{
	enum tcp_state_e state;
	u32 remotehost;
	u16 remoteport;
	u16 localport;
	u32 incoming_seq_no;
	u32 outgoing_seq_no;
	tcp_event_f * handler;
	tcp_buf * sendbuf;
} tcp_conn;

static tcp_conn tcp_conns[ TCP_MAX_CONNS ];

__inline tcp_sock tcp_sock_from_conn( tcp_conn * conn )
{
	return conn ? ((tcp_sock) (conn - tcp_conns)) : INVALID_TCP_SOCK;
}

__inline tcp_conn * tcp_conn_from_sock( tcp_sock sock )
{
	return (sock == INVALID_TCP_SOCK) ? 0 : &tcp_conns[ sock ];
}

static tcp_conn * tcp_find_unused()
{
	u08 i;
	for( i = 0; i < TCP_MAX_CONNS; i++ )
	{
		tcp_conn * p = &tcp_conns[ i ];
		if (!p->state)
			return p;
	}
	return NULL;
}

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

static void tcp_unbuffer( tcp_conn * conn, u32 bytes )
{
	while( bytes && conn->sendbuf )
	{
		u32 n = __min( bytes, conn->sendbuf->len - conn->sendbuf->ofs );
		
		if (n < conn->sendbuf->len - conn->sendbuf->ofs)
		{
			conn->sendbuf->ofs += n;
			bytes -= n;
		}
		else
		{
			// free this buffer chunk
			tcp_buf * b = conn->sendbuf;
			bytes -= n;
			conn->sendbuf = conn->sendbuf->next;

			conn->handler( tcp_sock_from_conn( conn ), ev_releasebuf, 
				(void *)b->data, b->len );

			free( b );	// would be really nice to make this non-heap-alloc'd too.
		}
	}
}

void tcp_sendpacket( tcp_conn * conn, void* data, u16 datalen, u08 flags )
{
	u08 iface;
	memset( &out, 0, sizeof( out ) );

	if (!arp_make_eth_header( &out.eth, conn->remotehost, &iface ))
		return;

	__ip_make_header( &out.ip, IPPROTO_TCP, 0, 
		datalen + sizeof( ip_header ) + sizeof( tcp_header ), conn->remotehost );

	out.tcp.ack_no = __htonl(conn->incoming_seq_no);
	out.tcp.seq_no = __htonl(conn->outgoing_seq_no);
	out.tcp.dest_port = conn->remoteport;
	out.tcp.src_port = conn->localport;
	out.tcp.data_offset = 5 << 4;
	out.tcp.flags = flags;
	out.tcp.urgent_pointer = 0;
	out.tcp.window = 0xffff;

	if( datalen )
		memcpy( out.crap, data, datalen );

	out.tcp.checksum = ~__htons(__checksum_ex( __pseudoheader_checksum( &out.ip ), 
		&out.tcp, datalen + sizeof( tcp_header ) ));

	__send_packet( iface, (u08*)&out, 
		sizeof( eth_header ) + sizeof( ip_header ) + sizeof( tcp_header ) + datalen );
}

void tcp_send_synack( tcp_conn * conn, tcp_header * inc_packet )
{
	conn->incoming_seq_no = __ntohl( inc_packet->seq_no ) + 1;
	tcp_sendpacket( conn, 0, 0, TCP_SYN | TCP_ACK );
}

void tcp_send_ack( tcp_conn * conn )
{
	tcp_sendpacket( conn, 0, 0, TCP_ACK );
}

void handle_listen_port( tcp_conn * conn, ip_header * p, tcp_header * t )
{
	tcp_conn * newconn = tcp_find_unused();
	assert( newconn );	// please!

	newconn->state = TCP_STATE_SYN;
	newconn->localport = conn->localport;
	newconn->remotehost = p->src_addr;
	newconn->remoteport = t->src_port;
	newconn->incoming_seq_no = __ntohl( t->seq_no );
	newconn->outgoing_seq_no = 0x400;
	newconn->handler = conn->handler;
	newconn->sendbuf = 0;

	logf( "flags: %x\n", t->flags );

	tcp_send_synack( newconn, t );
	newconn->handler( tcp_sock_from_conn( newconn ), ev_opened, 0, 0 );
}

u08 handle_connection( tcp_conn * conn, ip_header * p, tcp_header * t, u16 len )
{
	// todo: RST should be made to work :)
	u32 datalen = __ip_payload_length( p ) - (t->data_offset >> 2);

	if (conn->state == TCP_STATE_SYN)
	{
		conn->state = TCP_STATE_ESTABLISHED;
		return 1;
	}

	if( t->flags == TCP_ACK )
	{
		u32 drop = __ntohl(t->ack_no) - conn->outgoing_seq_no;	// todo: bob, check this
		if (drop)
			tcp_unbuffer( conn, drop );	// drop the stuff they acked from our buffer
	}

	if( datalen && __ntohl( t->seq_no ) == conn->incoming_seq_no )
	{
		conn->incoming_seq_no = __ntohl(t->seq_no) + datalen;
		conn->handler( tcp_sock_from_conn(conn), ev_data, __tcp_payload( t ), datalen );
		tcp_send_ack( conn );

		// todo: send outstanding data?
	}

	len;
	return 1;
}

u08 tcp_receive_packet( ip_header * p, u16 len )
{
	tcp_header * tcph;
	tcp_conn * conn;

	tcph = ( tcp_header * )__ip_payload( p );

	if( p->dest_addr != get_hostaddr() )
		return 0;

	conn = tcp_find_connection( p->src_addr, tcph->src_port, tcph->dest_port );
	if( conn && conn->state )
		return handle_connection( conn, p, tcph, len );

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

		handle_listen_port( conn, p, tcph );
		return 1;
	}
	return 0;
}

tcp_sock tcp_new_listen_sock( u16 port, tcp_event_f * handler )
{
	tcp_conn * conn;

	port = __htons( port );
	conn = tcp_find_listener( port );

	if( conn->state != TCP_STATE_CLOSED )
	{
		logf( "tcp: already listening on port %u", __ntohs( port ) );
		return INVALID_TCP_SOCK;
	}

	memset( conn, 0, sizeof( tcp_conn ) );
	conn->state = TCP_STATE_LISTENING;
	conn->localport = port;
	conn->handler = handler;
	conn->sendbuf = 0;

	return tcp_sock_from_conn( conn );
}

void tcp_send( tcp_sock sock, void* buf, u32 buf_len )
{
	tcp_conn * conn = tcp_conn_from_sock( sock );
	tcp_buf * p, * b;

	assert( conn );
	p = conn->sendbuf;

	if ( conn->state != TCP_STATE_ESTABLISHED )
		return;	// todo: are we allowed to do this at any other time?
	
	b = malloc( sizeof( tcp_buf ) );
	b->next = 0;
	b->ofs = 0;
	b->len = buf_len;
	b->data = buf;

	if (!conn->sendbuf)
		conn->sendbuf = b;
	else
	{
		while( p->next ) 
			p = p->next;

		p->next = b;
	}

	// todo: actually send data
}
