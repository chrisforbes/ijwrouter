// ijw router win32 stub

#include "../common.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"

int main( int argc, char ** argv )
{
	for(;;)
	{
		// simple test code - discards all incoming packets
		eth_packet p;
		if (eth_getpacket( &p ))
		{
			// todo: decide whether to forward it
			eth_discard( &p );
		}
	}
}