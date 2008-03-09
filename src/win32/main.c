// ijw router win32 stub

#include "../common.h"
#include "../ethernet.h"
#include "../ip.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"
#include "../user.h"

#include <memory.h>

#define IFACE_WAN		0x00
#define IFACE_BROADCAST	0xff

u08 mac_equal( mac_address const * a, mac_address const * b )
{
	return 0 == memcmp( a, b, sizeof(mac_address) );
}

static mac_address router_address;

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
		return eth_discard( p );

	if (u->credit >= size)
	{
		u->credit -= size;
		return eth_forward( p );
	}

	return eth_discard( p );	// do not want
}

u08 handle_packet( eth_packet * p )
{
	if (p->packet->ethertype == ethertype_arp)
	{
		logf( "+ arp\n" );
		return eth_forward( p );
	}

	if (p->packet->ethertype != ethertype_ipv4 )
	{
		logf( "- non-ip\n" );
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

int main( void )
{
	u08 interfaces = eth_init();

	if (!interfaces)
		logf( "! no interfaces available\n" );

	for(;;)
	{
		eth_packet p;
		if (eth_getpacket( &p ))
			handle_packet( &p );
	}
}