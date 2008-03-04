#ifndef ETHERNET_H
#define ETHERNET_H

/*
	802.3 framing
*/

typedef struct mac_address
{
	u08 bytes[6];
};

typedef struct mac_header
{
	mac_address dest;
	mac_address src;
	u16 ethertype;	
};

enum
{
	ethertype_ipv4	= 0x0800,
	ethertype_arp	= 0x0806,
	ethertype_rarp	= 0x8035,
	ethertype_appletalk = 0x809b,
	ethertype_aarp	= 0x80f3,
	ethertype_vlan	= 0x8100,				// 802.1Q tagged frame
	ethertype_ipx	= 0x8137,
	ethertype_novell = 0x8138,
	ethertype_ipv6	= 0x86dd,
	ethertype_mpls_unicast	= 0x8847,
	ethertype_mpls_multicast= 0x8848,
	ethertype_pppoe_disco	= 0x8863,
	ethertype_pppoe_session = 0x8864,
	ethertype_eap	= 0x888e,				// 802.1X
	ethertype_scsi	= 0x889a,
	ethertype_ata	= 0x88a2,
	ethertype_ethercat	= 0x88a4,
	ethertype_mac_security	= 0x88e5,		// 802.1AE
	ethertype_llt	= 0xcafe,				// veritas low-latency transport
};

#endif
