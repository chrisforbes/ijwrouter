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

void eth_inject_packet( u08 iface, u08 const * data, u16 len )
{
	eth_packet p;
	p.src_iface = IFACE_INTERNAL;
	p.dest_iface = iface;
	p.len = len;
	p.packet = (eth_header *)data;

	eth_inject( &p );
}

u08 handle_packet( eth_packet * p )
{
	u16 ethertype = __ntohs(p->packet->ethertype);

	// throw away packets from us (yes, we see them with winpcap!)
	if (mac_equal( p->packet->src, get_macaddr()))
		return eth_discard( p );

	dump_packet( p );

	if (p->dest_iface == IFACE_INTERNAL)
	{
		ipstack_receive_packet( p->src_iface, (u08 const *)p->packet, p->len );
		return eth_discard( p );	// dont want to forward it
	}

	if (ethertype == ethertype_arp)
	{
		if (ipstack_receive_packet( p->src_iface, (u08 const *)p->packet, p->len ))
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

	logf( "+ pure lan\n" );
	return eth_forward( p );	// not crossing from lan <-> wan	*/
}

int main( void )
{
	u08 interfaces = eth_init();

	set_hostaddr( make_ip( 192, 168, 2, 253 ) );
	set_netmask( make_ip( 255, 255, 255, 0 ) );

	ipstack_init( eth_inject_packet );

	if (!interfaces)
		logf( "! no interfaces available\n" );

	for(;;)
	{
		eth_packet p;
		if (eth_getpacket( &p ))
			handle_packet( &p );
	}
}