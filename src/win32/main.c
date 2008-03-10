// ijw router win32 stub

#pragma comment( lib, "ws2_32.lib" )
#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/conf.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"
#include "../user.h"
#include "../hal_time.h"
#include "../ip/rfc.h"

#include <uip/uip.h>
#include <uip/uip_arp.h>

#include "../ip/stack.h"

u08 charge_for_packet( eth_packet * p )
{
	mac_addr lanside = (p->dest_iface == IFACE_WAN) 
		? p->packet->dest : p->packet->src;

	user * u = get_user( lanside );

	if (!u)
	{
		logf("- (dropped, no user)\n");
		return eth_discard( p );
	}

	if (u->credit >= p->len)
	{
		u->credit -= p->len;
		logf("+ (ok)\n");
		return eth_forward( p );
	}
	
	logf("- (dropped, insufficient funds)\n");
	return eth_discard( p );	// do not want
}

u08 handle_packet( eth_packet * p )
{
	u16 ethertype = ntohs(p->packet->ethertype);

	if (mac_equal( p->packet->src, get_macaddr()))
		return eth_discard( p );

	dump_packet( p );

	if (p->dest_iface == IFACE_INTERNAL)
	{
		eth_uip_feed( p, (ethertype == ethertype_arp) ? 1 : 0 );
		return eth_discard( p );	// dont want to forward it
	}

	if (ethertype == ethertype_arp)
	{
		// ask uip whether it wants this packet
		if (eth_uip_feed( p, 1 ))
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
		logf( "- self-route\n" );
		return eth_discard( p );
	}

	// todo: re-enable this when we have an actual WAN interface

//	if ((p->src_iface == IFACE_WAN) || (p->dest_iface == IFACE_WAN))
//		return charge_for_packet( p );

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
			eth_uip_send( 0 );
	}

#if UIP_UDP
	for( i = 0; i < UIP_UDP_CONNS; i++ )
	{
		uip_udp_periodic(i);
		if (uip_len > 0)
			eth_uip_send( 0 );
	}
#endif

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

	set_hostaddr( make_ip( 192, 168, 2, 253 ) );
	set_netmask( make_ip( 255, 255, 255, 0 ) );

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