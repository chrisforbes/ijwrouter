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
	for( i = 0; i < sizeof( ip_header ); i++ )
		logf( "%02x", ((u08 *) (p->packet + 1))[i] );

	logf( "\n" );
}

u08 handle_packet( eth_packet * p )
{
	u16 ethertype = ntohs(p->packet->ethertype);

	dump_packet( p );

	if (p->dest_iface == IFACE_INTERNAL)
	{
		// TODO: feed the packet to uIP
		return eth_discard( p );	// dont want to forward it
	}

	if (ethertype == ethertype_arp)
	{
		logf( "+ arp\n" );
		return eth_forward( p );
	}

	if (ethertype != ethertype_ipv4 )
	{
		logf( "- non-ip (ethertype=%x)\n", ethertype );
		return eth_discard( p );
	}

	if (p->src_iface == p->dest_iface)
	{
		logf( "- self-route\n" );
		return eth_discard( p );
	}

	if ((p->src_iface == IFACE_WAN) || (p->dest_iface == IFACE_WAN))
		return charge_for_packet( p );

	logf( "+ pure lan\n" );
	return eth_forward( p );	// not crossing from lan <-> wan
}

void eth_uip_send(void)
{
	u08 buf[2048];
	eth_packet p;

	uip_arp_out();

	memcpy( buf, uip_buf, 40 + UIP_LLH_LEN );
	memcpy( buf + 40 + UIP_LLH_LEN, uip_appdata, uip_len - 40 - UIP_LLH_LEN );

	p.packet = (mac_header *) buf;
	p.src_iface = IFACE_INTERNAL;
	p.dest_iface = eth_find_interface( &p.packet->dest );
	p.len = uip_len;

	eth_inject( &p );
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
	u08 interfaces = eth_init();
	uip_init();

#pragma warning( disable: 4310 )
	uip_ipaddr( ipaddr, 192, 168, 2, 253 );
	uip_ipaddr( netmask, 255, 255, 255, 0 );
	uip_ipaddr( default_router, 192, 168, 2, 1 );

	uip_sethostaddr( ipaddr );
	uip_setnetmask( netmask );
	uip_setdraddr( default_router );

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