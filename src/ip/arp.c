#include "../common.h"
#include "rfc.h"
#include "arp.h"
#include "arptab.h"
#include "conf.h"
#include "../hal_debug.h"
#include "internal.h"

#pragma pack( push, 1 )
	static struct
	{
		eth_header eth;
		arp_header arp;
	} out;
#pragma pack( pop )

	static mac_addr all_zero_mac = {{0,0,0,0,0,0}};

static void make_arp( arp_header * arp, u08 op, u32 tpa, mac_addr tha )
{
	arp->hlen = 6;
	arp->plen = 4;
	arp->htype = __htons(1);
	arp->ptype = __htons(0x0800);
	arp->oper = __htons( op );
	arp->sha = get_macaddr();
	arp->spa = get_hostaddr();
	arp->tha = tha;
	arp->tpa = tpa;
}

static void send_arp_reply( u08 iface, arp_header * req )
{
	memset( &out, 0, sizeof( out ) );
	out.eth.dest = out.arp.tha = req->sha;
	out.eth.src = out.arp.sha = get_macaddr();
	out.eth.ethertype = __htons( ethertype_arp );

	make_arp( &out.arp, ARP_REPLY, req->spa, req->sha );

	__send_packet( iface, &out, sizeof( out ) );
}

extern const mac_addr broadcast_mac;

void send_arp_request( u08 iface, u32 ip )
{
	memset( &out, 0, sizeof( out ) );
	out.eth.dest = broadcast_mac;
	out.eth.src = out.arp.sha = get_macaddr();
	out.eth.ethertype = __htons( ethertype_arp );

	make_arp( &out.arp, ARP_REQUEST, ip, all_zero_mac );

	__send_packet( iface, &out, sizeof( out ) );
}

u08 handle_arp_packet( u08 iface, arp_header * arp )
{
	u16 op = __ntohs( arp->oper );

	if (op == ARP_REQUEST)
	{
		if (arp->tpa == arp->spa)	// gratuitous arp
		{
			logf( "arp: got gratuitous arp\n" );
			arptab_insert( iface, arp->spa, arp->sha );
			return 0;
		}

		if (arp->tpa == get_hostaddr())
		{
			logf( "arp: got request, replying.\n" );
			send_arp_reply( iface, arp );
			return 1;
		}

		logf( "arp: for someone else.\n" );
		return 0;
	}

	if (op == ARP_REPLY)
	{
		logf( "arp: got reply\n" );
		arptab_insert( iface, arp->spa, arp->sha );
		return 1;
	}

	logf( "arp: bogus oper=%d\n", op );
	return 0;
}

u08 arp_make_eth_header( eth_header * h, u32 destip, u08 * iface )
{	// returns 1 if successful; 0 if we needed to send some ARP instead.
	if (!arptab_query( iface, destip, &h->dest ))
	{
		logf( "arp: no arp cache for host %u.%u.%u.%u, refreshing...\n",
			destip & 0xff,
			destip >> 8 & 0xff,
			destip >> 16 & 0xff,
			destip >> 24 & 0xff);
		send_arp_request( 0xff, destip );
		return 0;
	}

	h->src = get_macaddr();
	h->ethertype = __htons( ethertype_ipv4 );

	return 1;
}
