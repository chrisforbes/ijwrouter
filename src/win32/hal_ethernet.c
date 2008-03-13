// ethernet hal stub for win32

#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/conf.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"

#include <stdio.h>
#include "minpcap.h"

#pragma comment( lib, "packet.lib" )
#pragma comment( lib, "wpcap.lib" )

static pcap_t * dev;
u08 buf[2048];

#define MAXBLOCKTIME	10

u08 eth_init( void )
{
	pcap_if_t * alldevs;
	pcap_if_t * d;
	char errbuf[ PCAP_ERRBUF_SIZE ];

	if (pcap_findalldevs( &alldevs, errbuf ) == -1)
	{
		logf( "error in pcap_findalldevs: %s\n", errbuf );
		return 0;
	}

	d = alldevs->next;	// hack hack hack: interface 2 = ethernet interface on odd-socks/valkyrie

	dev = pcap_open_live( d->name, 65536, 1, MAXBLOCKTIME, errbuf );

	if (!dev)
	{
		logf( "failed opening interface %s\n", d->name );
		return 0;
	}
	else
		logf( "opened interface %s\n", d->name );

	pcap_freealldevs( alldevs );

	return 1;
}

u08 eth_getpacket( eth_packet * p )
{
	struct pcap_pkthdr h;
	u08 const * data = pcap_next( dev, &h );

	if (!data)
		return 0;	// no packet

	if (h.len != h.caplen)
	{
		logf( "incomplete packet (dropped)!!\n" );
		return 0;
	}

	if (h.len > sizeof(buf))
	{
		logf( "hal: ethernet frame too large (%u bytes)\n", h.len );
		return 0;
	}

	memcpy( buf, data, h.len );
	p->packet = (eth_header *) buf;
	p->src_iface = IFACE_WAN;	// we have only one
	p->dest_iface = eth_find_interface( p->packet->dest );
	p->len = (u16)h.len;

	return 1;
}

u08 eth_discard( eth_packet * p )
{
	p;
	return 1;
}

u08 eth_forward( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_inject( eth_packet * p )
{
	if (-1 == pcap_sendpacket( dev, p->packet, p->len ))
		return 0;

	return 1;
}

const mac_addr broadcast_mac = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

u08 eth_find_interface( mac_addr dest )
{
	if (mac_equal( dest, get_macaddr() ))
		return IFACE_INTERNAL;

	if (mac_equal( dest, broadcast_mac ))
		return IFACE_BROADCAST;

	return IFACE_WAN;	// hack hack hack
}