#ifndef HAL_ETHERNET_H
#define HAL_ETHERNET_H

typedef struct eth_packet
{
	u08 src_iface;
	u08 dest_iface;
	u16 len;
	eth_header * packet;
} eth_packet;

// note: unless otherwise noted, HAL functions returning u08 
//	 return zero to indicate failure.

u08 eth_init( void );	// returns number of usable interfaces
u08 eth_getpacket( eth_packet * p );
u08 eth_discard( eth_packet * p );	
u08 eth_forward( eth_packet * p );	
u08 eth_inject( eth_packet * p );	
u08 eth_find_interface( mac_addr dest );

#define IFACE_WAN		0x00
#define IFACE_BROADCAST	0xff
#define IFACE_INTERNAL	0xfe

u08 eth_uip_feed( eth_packet * p, u08 isarp );
void eth_uip_send( u08 isarp );
void dump_packet( eth_packet * p );

#endif