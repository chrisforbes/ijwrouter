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
#define IFACE_LAN0		0x01
#define IFACE_LAN1		0x02
#define IFACE_LAN2		0x03
#define IFACE_LAN3		0x04

#define IFACE_BROADCAST	0xff
#define IFACE_INTERNAL	0xfe

void dump_packet( eth_packet * p );

#endif
