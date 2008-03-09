// ijw router win32 stub

#include "../common.h"
#include "../ethernet.h"
#include "../ip.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"
#include "../user.h"
#include "../hal_time.h"

#include <uip/uip.h>
#include <uip/uip_arp.h>

static mac_address router_address;
mac_address my_address = { { 0x00, 0x19, 0xe0, 0xff, 0x09, 0x08 } };

uip_ip4addr_t ipaddr;
uip_ip4addr_t netmask;
uip_ip4addr_t default_router;

u16 packet_size( eth_packet * p )
{
	ip_header const * h = (ip_header const *)(p->packet + 1);
	return h->totalLength;
}

u08 charge_for_packet( eth_packet * p )
{
	mac_address * lanside = (p->dest_iface == IFACE_WAN) 
		? &p->packet->dest : &p->packet->src;

	user * u = get_user( lanside );
	u16 size = packet_size( p );

	if (!u)
	{
		logf("- (dropped, no user)\n");
		return eth_discard( p );
	}

	if (u->credit >= size)
	{
		u->credit -= size;
		logf("+ (ok)\n");
		return eth_forward( p );
	}
	
	logf("- (dropped, insufficient funds)\n");
	return eth_discard( p );	// do not want
}

#define htons(x) ( (u16) (x<<8) | (x>>8) )

void dump_packet( eth_packet * p )
{
	static char srcbuf[64], destbuf[64];
	u08 i;

	mac_to_str( srcbuf, &p->packet->src );
	mac_to_str( destbuf, &p->packet->dest );

	logf( "src=%s@%02x dest=%s@%02x ethertype=%04x\n", srcbuf, p->src_iface, destbuf, p->dest_iface, ntohs(p->packet->ethertype) );
	for( i = 0; i < p->len; i++ )
		logf( "%02x", ((u08 *) (p->packet))[i] );

	logf( "\n" );
}

	u08 arp_reply[] = { 0x00, 0xa0, 0xd1, 0x65, 0x30, 0xe7,	// beedee's MAC
		0x00, 0x19, 0xe0, 0xff, 0x09, 0x08,		// router MAC
		0x08, 0x06,		// ethertype

		// arp packet
		0x00, 0x01, // tos	
		0x08, 0x00,	// ip	
		0x06, 0x04, 
		0x00, 0x02,	// reply

		0x00, 0x19, 0xe0, 0xff, 0x09, 0x08,		// sender mac
		192, 168, 2, 253,

		0x00, 0xa0, 0xd1, 0x65, 0x30, 0xe7,		// dest mac
		192, 168, 2, 10,
	
		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0, 0, 0,		0, 0 };	


// glue code between ethernet hal and uip ////

void eth_uip_send(void)
{
	u08 buf[2048];
	eth_packet p;

//	uip_arp_out();

	if (uip_len == 0)
		return;

	memcpy( buf, uip_buf, uip_len );

	p.packet = (mac_header *) buf;
	p.src_iface = IFACE_INTERNAL;
	p.dest_iface = eth_find_interface( &p.packet->dest );
	p.len = uip_len;
	uip_len = 0;

	logf( "sendlen=%d\n", p.len );

	dump_packet( &p );

/*	memcpy( buf, arp_reply, sizeof(arp_reply) );
	p.len = sizeof(arp_reply);

	dump_packet( &p );	*/

	eth_inject( &p );
}

u08 uip_feed( eth_packet * p, u08 isarp )
{
	u08 const * data = (u08 const *)p->packet;
	
	memcpy( uip_buf, data, p->len );
	uip_len = p->len;

	if (isarp)
		uip_arp_arpin();
	else
	{
		uip_arp_ipin();
		uip_input();
	}

	if (uip_len == 0)
		return 0;	// do not want

	eth_uip_send();
	return 1;
}

// end of glue ////

u08 handle_packet( eth_packet * p )
{
	u16 ethertype = ntohs(p->packet->ethertype);

	if (mac_equal( &p->packet->src, &my_address ))
		return eth_discard( p );

	dump_packet( p );

	if (p->dest_iface == IFACE_INTERNAL)
	{
		uip_feed( p, (ethertype == ethertype_arp) ? 1 : 0 );
		return eth_discard( p );	// dont want to forward it
	}

	if (ethertype == ethertype_arp)
	{
		// ask uip whether it wants this packet
		if (uip_feed( p, 1 ))
		{
			logf( "+ arp (eaten by uip)\n" );
			return eth_discard( p );
		}
		else
		{
			logf( "+ arp (forwarded)\n" );
			return eth_forward( p );
		}
	}

	if (ethertype != ethertype_ipv4 )
	{
		logf( "- non-ip (ethertype=%x)\n", ethertype );
		return eth_discard( p );
	}

	if (p->src_iface == p->dest_iface)
	{
//		logf( "- self-route\n" );
		return eth_discard( p );
	}

	if ((p->src_iface == IFACE_WAN) || (p->dest_iface == IFACE_WAN))
		return charge_for_packet( p );

	logf( "+ pure lan\n" );
	return eth_forward( p );	// not crossing from lan <-> wan	*/
}

void do_timeouts( void )
{
	static u32 last_arp_refresh	= 0;
	u32 t = ticks();
	int i;

	for( i = 0; i < UIP_CONNS; i++ )
	{
#pragma warning( disable: 4127 )
		uip_periodic( i );

		if (uip_len > 0)
			eth_uip_send();
	}

	// todo: udp

	if (t - last_arp_refresh >= 10000)
	{
		uip_arp_timer();
		last_arp_refresh = t;
	}
}

int main( void )
{
	uip_eth_addr e;
	u08 interfaces = eth_init();
	uip_init();

#pragma warning( disable: 4310 )
	uip_ipaddr( ipaddr, 192, 168, 2, 253 );
	uip_ipaddr( netmask, 255, 0, 0, 0 );
	uip_ipaddr( default_router, 192, 168, 2, 10 );

	uip_sethostaddr( ipaddr );
	uip_setnetmask( netmask );
	uip_setdraddr( default_router );

	e = *( uip_eth_addr * ) &my_address;
	uip_setethaddr( e );

	if (!interfaces)
		logf( "! no interfaces available\n" );

	for(;;)
	{
		eth_packet p;
		if (eth_getpacket( &p ))
			handle_packet( &p );

		do_timeouts();
	}
}