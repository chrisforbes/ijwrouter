#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "arptab.h"
#include "../hal_debug.h"
#include "../hal_time.h"
#include "internal.h"
#include "tcp.h"

#include <assert.h>

#define TCP_MAX_CONNS	40
#define MAXSEGSIZE		1024

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
	u32 flags;
	u32 seq;
	u32 last_send_time;
} tcp_buf;

typedef struct tcp_conn
{
	enum tcp_state_e state;
	u32 remotehost;
	u16 remoteport;
	u16 localport;
	u32 incoming_seq_no;
	tcp_event_f * handler;
	tcp_buf * sendbuf;
	void * user;
	u32 next_outgoing_seq_no;
} tcp_conn;

static u32 tcp_commit_sequence_range( tcp_conn * conn, u32 count )
{
	u32 ret = conn->next_outgoing_seq_no;
	conn->next_outgoing_seq_no += count;
	return ret;
}

#define TCP_RETRANSMIT_TIMEOUT 100

static tcp_conn tcp_conns[ TCP_MAX_CONNS ];

static __inline tcp_sock tcp_sock_from_conn( tcp_conn * conn )
{
	return conn ? ((tcp_sock) (conn - tcp_conns)) : INVALID_TCP_SOCK;
}

static __inline tcp_conn * tcp_conn_from_sock( tcp_sock sock )
{
	return (sock == INVALID_TCP_SOCK) ? 0 : &tcp_conns[ sock ];
}

void * tcp_get_user_data( tcp_sock sock )
{
	tcp_conn * conn = tcp_conn_from_sock( sock );
	return conn ? conn->user : 0;
}

void tcp_set_user_data( tcp_sock sock, void * ctx )
{
	tcp_conn * conn = tcp_conn_from_sock( sock );
	if( conn )
		conn->user = ctx;
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

#include "../pack1.h"
	static struct
	{
		eth_header eth;
		ip_header ip;
		tcp_header tcp;
		u08 crap[2048];
	} PACKED_STRUCT out;
#include "../packdefault.h"

static void tcp_unbuffer( tcp_conn * conn, u32 bytes )
{
	while( bytes && conn->sendbuf )
	{
		u32 n = __min( bytes, conn->sendbuf->len - conn->sendbuf->ofs );
		bytes -= n;

		if (n < conn->sendbuf->len - conn->sendbuf->ofs)
			conn->sendbuf->ofs += n;
		else
		{	// free this buffer chunk
			tcp_buf * b = conn->sendbuf;
			conn->sendbuf = conn->sendbuf->next;

			conn->handler( tcp_sock_from_conn( conn ), ev_releasebuf, 
				(void *)b->data, b->len, b->flags );

			free( b );
		}
	}
}

static void tcp_sendpacket_ex( tcp_conn * conn, void const * data, u16 datalen, 
					   u08 flags, u32 seq )
{
	u08 iface;
	memset( &out, 0, sizeof( out ) );

	if (!arp_make_eth_header( &out.eth, conn->remotehost, &iface ))
		return;

	__ip_make_header( &out.ip, IPPROTO_TCP, 0, 
		datalen + sizeof( ip_header ) + sizeof( tcp_header ), conn->remotehost );

	out.tcp.ack_no = (flags & TCP_ACK) ? __htonl(conn->incoming_seq_no) : 0;
	out.tcp.seq_no = __htonl(seq);
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

static void tcp_send_synack( tcp_conn * conn, tcp_header * inc_packet )
{
	conn->incoming_seq_no = __ntohl( inc_packet->seq_no ) + 1;
	tcp_sendpacket_ex( conn, 0, 0, TCP_SYN | TCP_ACK, tcp_commit_sequence_range( conn, 1 ) );
}

static void tcp_send_ack( tcp_conn * conn )
{
	tcp_sendpacket_ex( conn, 0, 0, TCP_ACK, conn->next_outgoing_seq_no );
}

static void tcp_send_buffer( tcp_conn * conn, tcp_buf * b )
{
	u32 ofs = b->ofs;
	for(;;)
	{
		u32 chunklen = __min( b->len - ofs, MAXSEGSIZE );
		if (!chunklen) return;

		tcp_sendpacket_ex( conn, (void const *)(b->data + ofs),
			(u16) chunklen, TCP_ACK, b->seq + ofs );
		ofs += chunklen;
	}
}

static void handle_listen_port( tcp_conn * conn, ip_header * p, tcp_header * t )
{
	tcp_conn * newconn;

	if (t->flags != TCP_SYN)
		return;

	newconn = tcp_find_unused();

	assert( newconn );

	newconn->state = TCP_STATE_SYN;
	newconn->localport = conn->localport;
	newconn->remotehost = p->src_addr;
	newconn->remoteport = t->src_port;
	newconn->incoming_seq_no = __ntohl( t->seq_no );
	newconn->next_outgoing_seq_no = 0x400;
	newconn->handler = conn->handler;
	newconn->sendbuf = 0;

	tcp_send_synack( newconn, t );
	newconn->handler( tcp_sock_from_conn( newconn ), ev_opened, 0, 0, 0 );
}

static void kill_connection( tcp_conn * conn )
{
	tcp_unbuffer( conn, 0xfffffffful );	// free all the buffers
	conn->handler( tcp_sock_from_conn( conn ), ev_closed, 0, 0, 0 );
	memset( conn, 0, sizeof( tcp_conn ) );
}

void tcp_close( tcp_sock sock )
{
	tcp_conn * conn = tcp_conn_from_sock( sock );
	if (conn->state == TCP_STATE_ESTABLISHED)
	{
		tcp_sendpacket_ex( conn, 0, 0, TCP_FIN | TCP_ACK, tcp_commit_sequence_range(conn, 1) );
		conn->state = TCP_STATE_CLOSING;
	}
	else
		log_printf( "tcp: tcp_close() on socket in bad state\n" );
}

static u32 get_earliest_unacked( tcp_conn const * conn )
{
	return conn->sendbuf ? 
		conn->sendbuf->seq + conn->sendbuf->ofs 
		: conn->next_outgoing_seq_no;
}

static u08 handle_connection( tcp_conn * conn, ip_header * p, tcp_header * t, u16 len )
{
	u32 datalen = __ip_payload_length( p ) - (t->data_offset >> 2);
	u32 drop; len;

	if (conn->state == TCP_STATE_SYN)
	{
		conn->state = TCP_STATE_ESTABLISHED;
		return 1;
	}

	if (t->flags & TCP_RST)
	{
		log_printf( "tcp: connection reset\n" );
		kill_connection( conn );
		return 1;
	}

	if( t->flags & TCP_ACK )
	{
		if (conn->state == TCP_STATE_LAST_ACK)
		{
			kill_connection( conn );
			return 1;
		}
		else if (0 != (drop = __ntohl(t->ack_no) - get_earliest_unacked(conn))) 
		{
			tcp_unbuffer( conn, drop );
			if( conn->sendbuf )
				conn->sendbuf->last_send_time = get_time() - TCP_RETRANSMIT_TIMEOUT;
		}
	}

	if (t->flags & TCP_FIN)
	{
		conn->incoming_seq_no++;
		tcp_send_ack(conn);
		if (conn->state == TCP_STATE_ESTABLISHED)
		{
			tcp_sendpacket_ex( conn, 0, 0, TCP_FIN | TCP_ACK, tcp_commit_sequence_range( conn, 1 ) );
			conn->state = TCP_STATE_LAST_ACK;
		}
		else
			kill_connection(conn);
		return 1;
	}

	if( datalen && __ntohl( t->seq_no ) == conn->incoming_seq_no )
	{
		conn->incoming_seq_no = __ntohl(t->seq_no) + datalen;
		conn->handler( tcp_sock_from_conn(conn), ev_data, __tcp_payload( t ), datalen, 0 );
		tcp_send_ack( conn );
	}
	
	return 1;
}

u08 tcp_receive_packet( ip_header * p, u16 len )
{
	tcp_conn * conn;
	tcp_header * tcph = ( tcp_header * )__ip_payload( p );

	if( p->dest_addr != get_hostaddr() )
		return 0;

	conn = tcp_find_connection( p->src_addr, tcph->src_port, tcph->dest_port );
	if( conn && conn->state )
		return handle_connection( conn, p, tcph, len );

	conn = tcp_find_listener( tcph->dest_port );
	if( conn && conn->state == TCP_STATE_LISTENING )
	{
		handle_listen_port( conn, p, tcph );
		return 1;
	}
	return 0;
}

tcp_sock tcp_new_listen_sock( u16 port, tcp_event_f * handler )
{
	tcp_conn * conn = tcp_find_listener( port = __htons( port ) );

	if( conn->state != TCP_STATE_CLOSED )
	{
		log_printf( "tcp: already listening on port %u", __ntohs( port ) );
		return INVALID_TCP_SOCK;
	}

	memset( conn, 0, sizeof( tcp_conn ) );
	conn->state = TCP_STATE_LISTENING;
	conn->localport = port;
	conn->handler = handler;
	conn->sendbuf = 0;

	return tcp_sock_from_conn( conn );
}

static void tcp_append_buffer( tcp_conn * conn, tcp_buf * b )
{
	if (!conn->sendbuf)
		conn->sendbuf = b;
	else
	{
		tcp_buf * p = conn->sendbuf;
		while( p->next )
			p = p->next;

		p->next = b;
	}
}

void tcp_send( tcp_sock sock, void const * buf, u32 buf_len, u32 flags )
{
	tcp_conn * conn = tcp_conn_from_sock( sock );
	tcp_buf * b;

	assert( conn );

	if ( conn->state != TCP_STATE_ESTABLISHED ) return;
	
	b = malloc( sizeof( tcp_buf ) );
	b->next = 0;	b->ofs = 0;		b->len = buf_len; 
	b->data = buf;	b->flags = flags;
	b->seq = tcp_commit_sequence_range( conn, buf_len );
	b->last_send_time = get_time();

	tcp_append_buffer( conn, b );
	tcp_send_buffer( conn, b );
}

u32 tcp_gethost( tcp_sock sock )
{
	return tcp_conn_from_sock( sock )->remotehost;
}

void tcp_process( u32 current_time )
{
	u08 i;
	for( i = 0; i < TCP_MAX_CONNS; i++ )
	{
		tcp_conn * conn = &tcp_conns[ i ];
		if( conn->state && conn->sendbuf && conn->sendbuf->last_send_time + TCP_RETRANSMIT_TIMEOUT <= current_time )
		{
			tcp_buf * b = conn->sendbuf;
			u32 chunklen = __min( b->len - b->ofs, MAXSEGSIZE );
			if(chunklen)
				tcp_sendpacket_ex( conn, (void const *)(b->data + b->ofs), (u16)chunklen, TCP_ACK, b->seq + b->ofs );
			b->last_send_time = current_time;
		}
	}
}
