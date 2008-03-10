#include "../common.h"
#include "rfc.h"
#include "arp.h"
#include "arptab.h"
#include "../hal_debug.h"

void handle_arp_packet( u08 iface, arp_header * arp )
{
	u16 op = __ntohs( arp->oper );

	if (op == ARP_REQUEST)
	{
		if (arp->tpa == arp->spa)	// gratuitous arp
		{
			arptab_insert( iface, arp->spa, arp->sha );
			return;
		}
	}

	if (op == ARP_REPLY)
	{
		arptab_insert( iface, arp->spa, arp->sha );
		return;
	}

	logf( "arp: bogus oper=%d\n", arp->oper );
}