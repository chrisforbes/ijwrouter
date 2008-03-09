#ifndef ETHERNET_H
#define ETHERNET_H

/*
	802.3 framing
*/

#include <uip/uip.h>

typedef struct mac_header
{
	uip_eth_addr dest;
	uip_eth_addr src;
	u16 ethertype;	
} mac_header;

typedef enum ethertype
{
	ethertype_ipv4	= 0x0800,
	ethertype_arp	= 0x0806,
	ethertype_rarp	= 0x8035,
	ethertype_ipv6	= 0x86dd,
} ethertype;

__inline u08 mac_equal( void const * a, void const * b )
{
	return 0 == memcmp( a, b, 6 );
}

#endif
