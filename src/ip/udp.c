#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "arp.h"
#include "udp.h"
#include "../hal_debug.h"
#include "internal.h"

u08 udp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	udp_header * udp = ( udp_header * ) __ip_payload( p );

	iface; p; len;

	logf( "udp: got packet sp=%u, dp=%u\n", 
		__ntohs( udp->src_port ), __ntohs( udp->dest_port ) );

	return 0;
}