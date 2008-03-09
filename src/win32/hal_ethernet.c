// ethernet hal stub for win32
// todo: make this work with pcap?

#include "../common.h"
#include "../ethernet.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"

#include <stdio.h>
#include <pcap.h>

#pragma comment( lib, "packet.lib" )
#pragma comment( lib, "wpcap.lib" )

static pcap_t * dev;
u08 buf[2048];

extern mac_address my_address;

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

	d = alldevs->next;	// ethernet interface on odd-socks

	dev = pcap_open_live( d->name, 65536, 1, 1000, errbuf );

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

	memcpy( buf, data, h.len );
	p->packet = (mac_header *) buf;
	p->src_iface = IFACE_WAN;	// we have only one
	p->dest_iface = eth_find_interface( &p->packet->dest );	// todo

	return 1;
}

u08 eth_discard( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_forward( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

u08 eth_inject( eth_packet * p )
{
	p;
	return 0;	// didnt work
}

static const mac_address broadcast_mac = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

u08 eth_find_interface( mac_address const * dest )
{
	if (mac_equal( dest, &my_address ))
		return IFACE_INTERNAL;

	if (mac_equal( dest, &broadcast_mac ))
		return IFACE_BROADCAST;

	return IFACE_WAN;	// hack hack hack
}