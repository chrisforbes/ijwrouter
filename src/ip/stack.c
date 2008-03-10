#include "../common.h"
#include "stack.h"
#include "arptab.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "../hal_debug.h"

static send_packet_f * on_send = 0;

void __send_packet( u08 iface, u08 const * buf, u16 len )
{
	if (on_send)
		on_send( iface, buf, len );
	else
		logf( "ip: trying to send packet but no driver bound.\n" );
}

void ipstack_init( send_packet_f * send_callback )
{
	on_send = send_callback;
	// todo: initialize some other things
}

void ipstack_tick( void )
{
	arptab_tick();
}

u08 ipstack_receive_packet( u08 iface, u08 const * buf, u16 len )
{
	eth_header * eth = (eth_header *) buf;
	u16 etype = __ntohs( eth->ethertype ); 

	len;

	switch( etype )
	{
	case ethertype_ipv4:
		{
			ip_header * ip = (ip_header *) (eth + 1);
			arptab_insert( iface, ip->src_addr, eth->src );
//			ip_receive_packet( ip, len - sizeof( eth_header ) );		// todo
			return 0;	// todo
		}

	case ethertype_arp:
			return handle_arp_packet( iface, (arp_header *) (eth + 1) );
	}

	return 0;
}