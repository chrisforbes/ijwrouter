#ifndef HAL_ETHERNET_H
#define HAL_ETHERNET_H

typedef struct eth_packet
{
	u08 iface;
	mac_header * packet;
} eth_packet;

u08 eth_getpacket( eth_packet * p );
u08 eth_discard( eth_packet * p );
u08 eth_forward( eth_packet * p );

#endif