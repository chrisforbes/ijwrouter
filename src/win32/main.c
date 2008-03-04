// ijw router win32 stub

#include "../common.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"

#define IFACE_WAN	0

int main( int argc, char ** argv )
{
	u08 interfaces = eth_init();

	if (!interfaces)
		logf( "! no interfaces available\n" );

	for(;;)
	{
		// simple test code - discards all incoming packets
		eth_packet p;
		if (eth_getpacket( &p ))
		{
			// todo: decide whether to forward it
			if (p.iface == IFACE_WAN)
			{
				// its from the internet (or from our local router)
			}
			else
			{
				// its from the lan side
			}

			eth_discard( &p );
		}
	}
}