#include "../common.h"
#include "rfc.h"
#include "conf.h"
#include "icmp.h"
#include "internal.h"	// to send responses back down into the stack
#include "../hal_debug.h"
#include "arptab.h"
#include "arp.h"

#pragma pack( push, 1 )
	static struct
	{
		eth_header eth;
		ip_header ip;
		icmp_header icmp;
		u08 crap[ 1536 - sizeof(eth_header) - sizeof(ip_header) - sizeof(icmp_header) ];
	} out;
#pragma pack( pop )

static void icmp_send_reply( u08 iface, ip_header * reqip, icmp_header * reqping, u16 len )
{
	memset( &out, 0, sizeof(out) );
	if (!arptab_query( 0, reqip->src_addr, &out.eth.dest ))
	{
		logf( "icmp: no arp cache for host, refreshing...\n" );
		send_arp_request( iface, reqip->src_addr );
		return;
	}

	out.eth.src = get_macaddr();
	out.eth.ethertype = __htons( ethertype_ipv4 );

	out.ip.version = 0x45;
	out.ip.tos = 0;
	out.ip.length = __htons(len);	// this is bugged!
	out.ip.fraginfo = 0;
	out.ip.ident = reqip->ident;
	out.ip.dest_addr = reqip->src_addr;
	out.ip.src_addr = reqip->dest_addr;
	out.ip.ttl = 128;
	out.ip.proto = IPPROTO_ICMP;
	out.ip.checksum = ~__htons(__checksum( &out.ip, sizeof(ip_header) ));

	out.icmp.type = 0;
	out.icmp.code = 0;
	out.icmp.sequence = reqping->sequence;
	out.icmp.id = reqping->id;
	memcpy( &out.crap, reqping + 1, len - sizeof( ip_header ) - sizeof( icmp_header ) );
	out.icmp.checksum = ~__htons(__checksum( &out.icmp, len - sizeof( ip_header ) ));

	__send_packet( iface, &out, len + sizeof( eth_header ) );
}

u08 icmp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	icmp_header * icmp = ( icmp_header * )__ip_payload( p );
	logf( "icmp: got packet, type=%d code=%d\n", icmp->type, icmp->code );

	if (icmp->type == 8 && icmp->code == 0)
		icmp_send_reply( iface, p, icmp, len );
	
	return 1;
}