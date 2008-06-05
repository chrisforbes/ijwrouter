// ethernet hal stub for win32

#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/conf.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"
#include "../stats.h"

#include <stdio.h>

#define _WINSOCKAPI_
#include <windows.h>

#include "minpcap.h"

#pragma comment( lib, "packet.lib" )
#pragma comment( lib, "wpcap.lib" )

// recently-sent buffer, per interface. so we dont see our own transmissions..
typedef struct buf_t
{
	u08 buf[2048];
	u32 len;
} buf_t;

#define RSB_SIZE	8

typedef struct rsb_t
{
	buf_t packets[RSB_SIZE];
	u08 head, tail;
} rsb_t;

void rsb_insert( rsb_t * rsb, eth_packet * p )
{
	if (++rsb->head >= RSB_SIZE) rsb->head = 0;
	if (rsb->tail == rsb->head)
		if (++rsb->tail >= RSB_SIZE) rsb->tail = 0;

	memcpy( rsb->packets[ rsb->head ].buf, p->packet, p->len );
	rsb->packets[ rsb->head ].len = p->len;
}

u08 rsb_check( rsb_t * rsb, eth_packet * p )
{
	u08 x = rsb->tail;
	while( x != rsb->head )
	{
		if (0 == memcmp( rsb->packets[x].buf, p->packet, __min( p->len, rsb->packets[x].len ) ) )
			return 1;
		if (++x >= RSB_SIZE) x = 0;
	}

	return 0;
}

#define MAXINTERFACES	5
static pcap_t * interfaces[MAXINTERFACES];
HANDLE interface_handles[MAXINTERFACES];
static rsb_t sendbuf[MAXINTERFACES];
int num_interfaces;
u08 buf[2048];

#define MAXBLOCKTIME	10

static void * in_packet_counter = 0;
static void * out_packet_counter = 0;

u08 eth_init_interface( u08 iface, u08 real_iface )
{
	pcap_if_t * alldevs;
	pcap_if_t * d;
	char errbuf[ PCAP_ERRBUF_SIZE ];

	interfaces[ iface ] = 0;

	if (pcap_findalldevs( &alldevs, errbuf ) == -1)
	{
		logf( "hal: error in pcap_findalldevs: %s\n", errbuf );
		return 0;
	}

	if (!alldevs)
	{
		logf( "hal: looks like there's no network cards, or pcap isnt running.\n" );
		return 0;
	}

	d = alldevs;
	while( real_iface-- )
		d = d->next;

	interfaces[ iface ] = pcap_open_live( d->name, 65536, 1, 1000, errbuf );

	if (!interfaces[iface])
	{
		logf( "hal: failed opening interface %s\n", d->name );
		return 0;
	}

	logf( "hal: iface=%d -> %s\n", 
		iface, d->description );

	pcap_setmintocopy( interfaces[iface], 1 );

	interface_handles[ iface ] = (HANDLE)pcap_getevent( interfaces[ iface ] );

	pcap_freealldevs( alldevs );
	return 1;
}

static u08 __eth_init_interface( u08 internal_id, char const * keyname )
{
	u08 result = (u08)GetPrivateProfileIntA( "interfaces", 
		keyname, 0x0ff, "../ijw-router-win32.ini" );
	if (result == 0xff)
		return 0;

	if (eth_init_interface( internal_id, result ))
	{
		num_interfaces = internal_id + 1;
		return 1;
	}

	return 0;
}

u08 eth_init( void )
{
	struct iface_info_t { u08 id; char const * name; } iface_infos[] = 
	{
		{ IFACE_WAN, "wan" },
		{ IFACE_LAN0, "lan0" },
		{ IFACE_LAN1, "lan1" },
		{ IFACE_LAN2, "lan2" },
		{ IFACE_LAN3, "lan3" },
		{ 0, 0 }
	};

	struct iface_info_t const * p = iface_infos;
	while( p->name )
	{
		if (!__eth_init_interface( p->id, p->name ))
			break;
		++p;
	}

	in_packet_counter = stats_new_counter("Total Incoming Packets");
	out_packet_counter = stats_new_counter("Total Outgoing Packets");

	return 1;
}

u08 eth_getpacket( eth_packet * p )
{
	struct pcap_pkthdr h;
	u08 const * data;

	u08 iface;
	DWORD obj = WaitForMultipleObjects( num_interfaces, interface_handles, FALSE, MAXBLOCKTIME );
	
	if (obj == WAIT_FAILED)
	{
		logf( "hal: WaitForMultipleObjects() failed with %u\n", GetLastError() );
		return 0;
	}

	if (obj == WAIT_TIMEOUT)
	{
		//logf( "hal: WaitForMultipleObject() timed out\n" );
		return 0;		// there was no packet
	}

	iface = (u08)(obj - WAIT_OBJECT_0);

	if ( !interfaces[iface] )
	{
		logf( "hal: no interface for iface=%d\n", iface );
		return 0;	// no interface??
	}

	data = pcap_next( interfaces[iface], &h );

	if ( !data )
		return 0;	// no packet

	if (h.len != h.caplen)
	{
		logf( "hal: incomplete ethernet frame (%u from %u)\n", 
			h.caplen, h.len );
		return 0;
	}

	if (h.len > sizeof(buf))
	{
		logf( "hal: ethernet frame too large (%u bytes)\n", h.len );
		return 0;
	}

	stats_inc_counter(in_packet_counter, 1);

	memcpy( buf, data, h.len );
	p->packet = (eth_header *) buf;
	p->src_iface = iface;
	p->dest_iface = eth_find_interface( p->packet->dest );
	p->len = (u16)h.len;

	if (rsb_check( &sendbuf[iface], p ))
		return 0;	// our own transmission!

	return 1;
}

u08 eth_discard( eth_packet * p )
{
	p;
	return 1;
}

u08 eth_forward( eth_packet * p )
{
	
	return eth_inject(p);
}

u08 eth_inject( eth_packet * p )
{
	if (p->dest_iface == p->src_iface) return 0;	// dont allow route back to source port

	if ( p->dest_iface == IFACE_BROADCAST )
	{
		for( p->dest_iface = IFACE_WAN; p->dest_iface < num_interfaces; p->dest_iface++ )
			eth_inject( p );

		stats_inc_counter(out_packet_counter, 1);
		return 1;
	}

	if (p->dest_iface == IFACE_INTERNAL )
	{
		logf( "hal: tried to inject packet into IFACE_INTERNAL\n" );
		return 0;
	}

	if (-1 == pcap_sendpacket( interfaces[ p->dest_iface ], p->packet, p->len ))
		return 0;

	rsb_insert( &sendbuf[p->dest_iface], p );
	stats_inc_counter(out_packet_counter, 1);

	return 1;
}

const mac_addr broadcast_mac = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

u08 eth_find_interface( mac_addr dest )
{
	if (mac_equal( dest, get_macaddr() ))
		return IFACE_INTERNAL;

	if (mac_equal( dest, broadcast_mac ))
		return IFACE_BROADCAST;

	return IFACE_WAN;
}
