#include <string.h>

#include "common.h"
#include "hal_debug.h"
#include "hal_time.h"

#include "ip/rfc.h"
#include "ip/udp.h"
#include "ip/conf.h"

#pragma warning( disable: 4127 )
#pragma warning( disable: 4310 )

typedef enum dhcp_state_e
{
	DHCP_STATE_IDLE = 0,
	DHCP_STATE_WAIT_FOR_OFFER = 1,
	DHCP_STATE_WAIT_FOR_ACK = 2,
	DHCP_STATE_DONE = 3,
} dhcp_state_e;

typedef struct dhcp_state
{
	u08 state;
	udp_sock socket;
	u32 serverid;
	u32 lease_time;
	u32 send_time;
} dhcp_state;

static dhcp_state s = { 0, 0, 0, 0, 0 };

typedef struct dhcp_packet
{
	u08 op, htype, hlen, hops;
	u32 xid;	// random number identifying transaction
	u16 secs, flags;

	u32 ciaddr;
	u32 yiaddr;
	u32 siaddr;
	u32 giaddr;
	mac_addr chaddr;
	u08 crap[10];
	u08 sname[64];
	u08 file[128];

	u08 options[312];
} dhcp_packet;

#define BOOTP_BROADCAST	0x8000

#define DHCP_OP_REQUEST	1
#define DHCP_OP_REPLY	2

#define DHCP_HTYPE_ETHERNET	1
#define DHCP_HLEN_ETHERNET	6
#define DHCP_MSG_LEN	236

#define DHCP_SERVER_PORT	((u16)67)
#define DHCP_CLIENT_PORT	((u16)68)

#define DHCP_TIMEOUT	1000

typedef enum dhcp_op
{
	DHCP_DISCOVER = 1,
	DHCP_OFFER = 2,
	DHCP_REQUEST = 3,
	DHCP_DECLINE = 4,
	DHCP_ACK = 5,
	DHCP_NAK = 6,
	DHCP_RELEASE = 7,
} dhcp_op;

typedef enum dhcp_option
{
	DHCP_OPTION_NETMASK = 1,
	DHCP_OPTION_ROUTER = 3,
	DHCP_OPTION_DNS_SERVER = 6,
	DHCP_OPTION_HOSTNAME = 12,
	DHCP_OPTION_REQ_IPADDR = 50,
	DHCP_OPTION_LEASE_TIME = 51,
	DHCP_OPTION_MSG_TYPE = 53,
	DHCP_OPTION_SERVER_ID = 54,
	DHCP_OPTION_REQ_LIST = 55,
	DHCP_OPTION_END = 255
} dhcp_option;

static const u32 xid = 0xadde1223;
static const u08 cookie[] = { 0x63, 0x82, 0x53, 0x63 };

static u08 * append_opt( u08 * p, u08 optname, u08 len, u08 const * option )
{
	*p++ = optname;
	if (len)
	{
		*p++ = len;
		memcpy( p, option, len );
	}
	return p + len;
}

#define DEF_APPEND_OPT_T( T )\
	static u08 * append_opt_ ## T ( u08 * p, u08 optname, T value )\
		{ return append_opt( p, optname, sizeof(value), (u08 *)&value ); }

static u08 * append_opt_void( u08 * p, u08 optname )
{ 
	return append_opt( p, optname, 0, 0 ); 
}

DEF_APPEND_OPT_T( u08 )
DEF_APPEND_OPT_T( u32 )

static void create_msg( dhcp_packet * p )
{
	memset( p, 0, sizeof( dhcp_packet ) );
	p->op = DHCP_OP_REQUEST;
	p->htype = DHCP_HTYPE_ETHERNET;
	p->hlen = DHCP_HLEN_ETHERNET;
	p->xid = xid;
	p->flags = __htons( BOOTP_BROADCAST );
	p->chaddr = get_macaddr();
	memset(p->crap, 0, sizeof(p->crap));
	memcpy( p->options, cookie, sizeof(cookie) );
}

static u32 tickCount = 0;

static void send_discover( void )
{
	dhcp_packet p;
	u08 * start = (u08 *)&p;
	u08 * end = p.options + 4;
	u08 const * hostname = get_hostname();
	u08 opts[] = { DHCP_OPTION_NETMASK, DHCP_OPTION_ROUTER, DHCP_OPTION_DNS_SERVER };

	create_msg( &p );

	end = append_opt_u08( end, DHCP_OPTION_MSG_TYPE, DHCP_DISCOVER );
	end = append_opt( end, DHCP_OPTION_REQ_LIST, sizeof(opts), opts );
	end = append_opt( end, DHCP_OPTION_HOSTNAME, (u08)strlen( (char const *)hostname ) + 1, hostname );
	end = append_opt_void( end, DHCP_OPTION_END );

	udp_send( s.socket, 0xfffffffful, DHCP_SERVER_PORT, start, (u16)(end - start) );

	logf( "dhcp: discover sent\n" );
	tickCount = ticks();
	s.state = DHCP_STATE_WAIT_FOR_OFFER;
}

static void send_request( void )
{
	dhcp_packet p;
	u08 * start = (u08 *)&p;
	u08 * end = p.options + 4;
	u08 const * hostname = get_hostname();

	create_msg( &p );
	
	end = append_opt_u08( end, DHCP_OPTION_MSG_TYPE, DHCP_REQUEST );
	end = append_opt_u32( end, DHCP_OPTION_SERVER_ID, s.serverid );
	end = append_opt( end, DHCP_OPTION_HOSTNAME, (u08)strlen( (char const *)hostname ) + 1, hostname );
	end = append_opt_u32( end, DHCP_OPTION_REQ_IPADDR, get_hostaddr());

	udp_send( s.socket, 0xfffffffful, DHCP_SERVER_PORT, start, (u16)(end-start));
	tickCount = ticks();
	s.state = DHCP_STATE_WAIT_FOR_ACK;
}

static u08 parse_options( u08 * opt, int len )
{
	u08 * end = opt + len;
	u08 type = 0;

	while( opt < end )
	{
		switch( *opt )
		{

		case DHCP_OPTION_MSG_TYPE:
			type = *( u08 *) (opt + 2);
			break;

		case DHCP_OPTION_NETMASK:
			if (type == DHCP_ACK)
				set_netmask( *( u32 * ) (opt + 2) );
			break;

		case DHCP_OPTION_SERVER_ID:
			s.serverid = *( u32 * ) (opt + 2);
			break;

		case DHCP_OPTION_LEASE_TIME:
			s.lease_time = *( u32 * ) (opt + 2);
			break;

		case DHCP_OPTION_ROUTER:
			if (type == DHCP_ACK)
				set_default_router( *( u32 * ) (opt + 2) );
			break;

		case DHCP_OPTION_END:
			return type;
		}

		opt += opt[1] + 2;
	}
	return type;
}

static u08 parse_msg( dhcp_packet * packet, u16 len )
{
	if (packet->op == DHCP_OP_REPLY &&
		packet->xid == xid)
	{
		set_hostaddr( packet->yiaddr );
		return parse_options( packet->options + 4, len );
	}

	return 0;
}

void dhcp_process( void )
{
	if ((ticks() - tickCount) >= DHCP_TIMEOUT)
	{
		switch(s.state)
		{
		case DHCP_STATE_WAIT_FOR_ACK:
			send_request();
			break;
		case DHCP_STATE_WAIT_FOR_OFFER:
			send_discover();
			break;
		}
	}
}

extern void sntp_init( void );	// set after dhcp completes

static void dhcp_event( udp_sock sock, udp_event_e evt, 
					   u32 from_ip, u16 from_port, u08 const * data, u16 len )
{
	u08 type = parse_msg((dhcp_packet *)data, len);
	from_ip; from_port;	evt; sock;

	if (s.state == DHCP_STATE_DONE)
		return;	// todo: handle expiry properly

	switch (type)
	{
	case DHCP_OFFER:
		logf( "dhcp: got offer\n" );
		s.state = DHCP_STATE_IDLE;
		send_request();
		break;
	case DHCP_ACK:
		logf( "dhcp: got ack\n" );
		logf( "dhcp: host configuration complete\n" );
		s.state = DHCP_STATE_DONE;	// todo: start lease timer
		sntp_init();
		break;
	case DHCP_NAK:
		logf( "dhcp: got nak\n" );
		s.state = DHCP_STATE_IDLE;
		send_discover();
		break;
	}
}

void dhcp_init( void )
{
	s.socket = udp_new_sock(DHCP_CLIENT_PORT, 0, dhcp_event);
	if (s.socket == INVALID_UDP_SOCK)
	{
		logf( "dhcp: unable to bind socket\n" );
		return;
	}

	logf( "dhcp: started successfully\n" );

	send_discover();
}
