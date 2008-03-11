#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "../hal_debug.h"
#include "internal.h"

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

static tcp_conn * tcp_find_conn( u16 port )
{
	tcp_conn * free = 0;
	u08 i;
	for( i = 0; i < UDP_MAX_CONNS; i++ )
	{
		tcp_conn * p = &tcp_conns[ i ];
		if (!p->state)
			free = p;
		else if (p->port == port)
			return p;
	}

	return free;
}

u08 tcp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	iface; p; len;
	logf( "tcp: got packet\n" );

	if( p->dest_addr != get_hostaddr() )
		return 1;

	return 0;
}
