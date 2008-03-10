#include "common.h"
#include "hal_debug.h"
#include <uip/uip.h>

#pragma warning( disable: 4127 )
#pragma warning( disable: 4310 )

typedef enum dhcp_state_e
{
	DHCP_STATE_INITIAL = 0,
	DHCP_STATE_WAIT_FOR_OFFER = 1,
	DHCP_STATE_WAIT_FOR_ACK = 2,
} dhcp_state_e;

typedef struct dhcp_state
{
	u08 state;
	struct uip_udp_conn * conn;
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
	u08 chaddr[16];

	u08 options[312];
} dhcp_packet;

#define BOOTP_BROADCAST	0x8000

#define DHCP_OP_REQUEST	1
#define DHCP_OP_REPLY	2

#define DHCP_HTYPE_ETHERNET	1
#define DHCP_HLEN_ETHERNET	6
#define DHCP_MSG_LEN	236

#define DHCP_SERVER_PORT	67
#define DHCP_CLIENT_PORT	68

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
	DHCP_OPTION_REQ_IPADDR = 50,
	DHCP_OPTION_LEASE_TIME = 51,
	DHCP_OPTION_MSG_TYPE = 53,
	DHCP_OPTION_SERVER_ID = 54,
	DHCP_OPTION_REQ_LIST = 55,
	DHCP_OPTION_END = 255
} dhcp_option;

static const u32 xid = 0xadde1223;
static const u08 cookie[] = { 99, 130, 83, 99 };

static u08 * append_opt( u08 * p, u08 optname, u08 len, u08 * option )
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

extern uip_eth_addr my_address;

static void create_msg( dhcp_packet * p )
{
	p->op = DHCP_OP_REQUEST;
	p->htype = DHCP_HTYPE_ETHERNET;
	p->hlen = DHCP_HLEN_ETHERNET;
	p->hops = 0;
	p->xid = xid;
	p->flags = HTONS( BOOTP_BROADCAST );

	p->ciaddr = __gethostaddr();
	p->yiaddr = 0;
	p->siaddr = 0;
	p->giaddr = 0;
	memcpy( p->chaddr, &my_address, sizeof( uip_eth_addr ) );
	memset( p->chaddr + sizeof( uip_eth_addr ), 0, 16 - sizeof( uip_eth_addr ) );
	memcpy( p->options, cookie, sizeof(cookie) );
}

static void send_discover( void )
{
	dhcp_packet * p = (dhcp_packet *)(uip_appdata = uip_buf + UIP_IPUDPH_LEN);
	u08 * end = p->options + 4;
	u08 opts[] = { DHCP_OPTION_NETMASK, DHCP_OPTION_ROUTER, DHCP_OPTION_DNS_SERVER };

	create_msg( p );

	end = append_opt_u08( end, DHCP_OPTION_MSG_TYPE, DHCP_DISCOVER );
	end = append_opt( end, DHCP_OPTION_REQ_LIST, sizeof(opts), opts );
	end = append_opt_void( end, DHCP_OPTION_END );

	uip_send( uip_appdata, end - (u08 *)uip_appdata );

	logf( "DHCPDISCOVER sent\n" );
}

static void send_request( void )
{
	dhcp_packet * p = (dhcp_packet *)uip_appdata;
	u08 * end = p->options + 4;

	create_msg( p );
	
	end = append_opt_u08( end, DHCP_OPTION_MSG_TYPE, DHCP_REQUEST );
	end = append_opt_u32( end, DHCP_OPTION_SERVER_ID, s.serverid );
	end = append_opt_u32( end, DHCP_OPTION_REQ_IPADDR, __gethostaddr() );

	uip_send( uip_appdata, end - (u08 *)uip_appdata );
}

static u08 parse_options( u08 * opt, int len )
{
	u08 * end = opt + len;
	u08 type = 0;

	while( opt < end )
	{
		switch( *opt )
		{
		case DHCP_OPTION_NETMASK:
			__setnetmask( *( u32 * ) (opt + 2) );
			break;

		case DHCP_OPTION_MSG_TYPE:
			type = *( u08 *) (opt + 2);
			break;

		case DHCP_OPTION_SERVER_ID:
			s.serverid = *( u32 * ) (opt + 2);
			break;

		case DHCP_OPTION_LEASE_TIME:
			s.lease_time = *( u32 * ) (opt + 2);
			break;

		case DHCP_OPTION_END:
			return type;
		}

		opt += opt[1] + 2;
		return type;
	}
}

static u08 parse_msg( void )
{
	dhcp_packet * m = (dhcp_packet *) uip_appdata;
	if (m->op == DHCP_OP_REPLY &&
		m->xid == xid &&
		memcmp( m->chaddr, &my_address, sizeof(my_address) ) == 0 )
	{
		__sethostaddr( m->yiaddr );
		return parse_options( m->options + 4, uip_datalen() );
	}

	return 0;
}

void dhcp_process( void )
{
}

void dhcp_init( void )
{
	u32 broadcast = 0xfffffffful;
	s.conn = uip_udp_new( (uip_ipaddr_t *) &broadcast, HTONS( DHCP_SERVER_PORT ) );
	if (!s.conn)
	{
		logf( "dhcp client failed\n" );
		return;
	}

	uip_udp_bind( s.conn, HTONS( DHCP_CLIENT_PORT ) );
	bind_handler( s.conn, dhcp_process );

	logf( "dhcp client started\n" );

	send_discover();
}


