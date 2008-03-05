// ethernet hal stub for win32
// todo: make this work with pcap?

#include "../common.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"

u08 eth_init( void )
{
	return 0;
}

u08 eth_getpacket( eth_packet * p )
{
	p;
	return 0;	// no packet
}

u08 eth_discard( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_forward( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_inject( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_find_interface( mac_address const * dest )
{
	dest;
	return 0;	// hax
}