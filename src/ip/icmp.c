#include "../common.h"
#include "rfc.h"
#include "conf.h"
#include "icmp.h"
#include "internal.h"	// to send responses back down into the stack
#include "../hal_debug.h"
#include "arptab.h"
#include "arp.h"

#include "../pack1.h"
static struct
{
	eth_header eth;
	ip_header ip;
	icmp_header icmp;
	u08 crap[ 1536 - sizeof(eth_header) - sizeof(ip_header) - sizeof(icmp_header) ];
} PACKED_STRUCT out;
#include "../packdefault.h"

static void icmp_send_reply( ip_header * reqip, icmp_header * reqping, u16 len )
{
	u08 iface;
	memset( &out, 0, sizeof(out) );

	if (!arp_make_eth_header( &out.eth, reqip->src_addr, &iface ))
		return;

	__ip_make_response( &out.ip, reqip, len );

	out.icmp.type = 0;
	out.icmp.code = 0;
	out.icmp.sequence = reqping->sequence;
	out.icmp.id = reqping->id;
	memcpy( &out.crap, reqping + 1, len - sizeof( ip_header ) - sizeof( icmp_header ) );
	out.icmp.checksum = ~__htons(__checksum( &out.icmp, len - sizeof( ip_header ) ));

	__send_packet( iface, &out, len + sizeof( eth_header ) );
}

u08 icmp_receive_packet( ip_header * p, u16 len )
{
	icmp_header * icmp = ( icmp_header * )__ip_payload( p );
	log_printf( "icmp: got packet, type=%d code=%d\n", icmp->type, icmp->code );

	if (icmp->type == 8 && icmp->code == 0)
		icmp_send_reply( p, icmp, len );
	
	return 1;
}
