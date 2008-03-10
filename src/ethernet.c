#include "common.h"
#include "ethernet.h"
#include "hal_ethernet.h"
#include "hal_debug.h"

#include <uip/uip.h>
#include <uip/uip_arp.h>

char * mac_to_str( char * buf, uip_eth_addr const * a )
{
	sprintf( buf, "%02x-%02x-%02x-%02x-%02x-%02x", 
		a->addr[0], a->addr[1], a->addr[2],
		a->addr[3], a->addr[4], a->addr[5] );
	return buf;
}

void dump_packet( eth_packet * p )
{
	static char srcbuf[64], destbuf[64];

	mac_to_str( srcbuf, &p->packet->src );
	mac_to_str( destbuf, &p->packet->dest );

	logf( "src=%s@%02x dest=%s@%02x type=%04x len=%u\n", 
		srcbuf, p->src_iface, 
		destbuf, p->dest_iface, 
		ntohs(p->packet->ethertype), p->len );
}

void eth_uip_send( u08 isarp )
{
	u08 buf[2048];
	eth_packet p;

	if (!isarp)
		uip_arp_out();

	if (uip_len == 0)
		return;

	memcpy( buf, uip_buf, uip_len );

	p.packet = (mac_header *) buf;
	p.src_iface = IFACE_INTERNAL;
	p.dest_iface = eth_find_interface( &p.packet->dest );
	p.len = uip_len;
	uip_len = 0;

	dump_packet( &p );

	eth_inject( &p );
}

u08 eth_uip_feed( eth_packet * p, u08 isarp )
{
	u08 const * data = (u08 const *)p->packet;
	
	memcpy( uip_buf, data, p->len );
	uip_len = p->len;

	if (isarp)
		uip_arp_arpin();
	else
	{
		uip_arp_ipin();
		uip_input();
	}

	if (uip_len == 0)
		return 0;	// do not want

	eth_uip_send( isarp );
	return 1;
}