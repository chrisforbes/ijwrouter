#ifndef HAL_ETHERNET_H
#define HAL_ETHERNET_H

typedef struct eth_packet
{
	u08 iface;
	mac_header * packet;
} eth_packet;

// note: unless otherwise noted, HAL functions returning u08 
//	 return zero to indicate failure.

u08 eth_init( void );	// returns number of usable interfaces
u08 eth_getpacket( eth_packet * p );
u08 eth_discard( eth_packet * p );	
u08 eth_forward( eth_packet * p );	
u08 eth_inject( eth_packet * p );	

#endif