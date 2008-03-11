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

static void send_arp_reply( u08 iface, arp_header * req )
{
	memset( &out, 0, sizeof( out ) );
	out.eth.dest = out.arp.tha = req->sha;
	out.eth.src = out.arp.sha = get_macaddr();
	out.eth.ethertype = __htons( ethertype_arp );
	out.arp.hlen = 6;
	out.arp.htype = __htons(1);
	out.arp.oper = __htons(ARP_REPLY);
	out.arp.plen = 4;
	out.arp.ptype = __htons( 0x0800 );
	out.arp.tpa = req->spa;
	out.arp.spa = req->tpa;

	__send_packet( iface, (u08 const *) &out, sizeof( out ) );
}

extern const mac_addr broadcast_mac;

void send_arp_request( u08 iface, u32 ip )
{
	memset( &out, 0, sizeof( out ) );
	out.eth.dest = broadcast_mac;
	out.eth.src = out.arp.sha = get_macaddr();
	out.eth.ethertype = __htons( ethertype_arp );
	out.arp.hlen = 6;
	out.arp.htype = __htons(1);
	out.arp.oper = __htons(ARP_REPLY);
	out.arp.plen = 4;
	out.arp.ptype = __htons( 0x0800 );
	out.arp.tpa = ip;
	out.arp.spa = get_hostaddr();

	__send_packet( iface, (u08 const *) &out, sizeof( out ) );
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