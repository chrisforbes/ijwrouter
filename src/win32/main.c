// ijw router win32 stub

#include "../common.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"

#include <memory.h>

#define IFACE_WAN	0

u08 mac_equal( mac_address const * a, mac_address const * b )
{
	return 0 == memcmp( a, b, sizeof(mac_address) );
}

static mac_address router_address;

u08 handle_packet( eth_packet * p )
{
	if (p->iface == IFACE_WAN)
	{
		// Don't worry sir! I'm from the Internet!
		return eth_discard( p );	// do not want.
	}

	if (!mac_equal( &router_address, &p->packet->dest ))
		return eth_forward( p );	// destined for another lan port

	// todo:
	return eth_discard( p );
}

int main( int argc, char ** argv )
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