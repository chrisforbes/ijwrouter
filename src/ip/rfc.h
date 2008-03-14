#ifndef RFC_H
#define RFC_H

#pragma pack( push, 1 )

typedef enum ethertype
{
	ethertype_ipv4	= 0x0800,
	ethertype_arp	= 0x0806,
	ethertype_rarp	= 0x8035,
	ethertype_ipv6	= 0x86dd,
} ethertype;

typedef struct eth_header
{
	mac_addr dest;
	mac_addr src;
	u16 ethertype;
} eth_header;

typedef struct ip_header
{
	u08 version;	// also contains header length in words
	u08 tos;
	u16 length;
	u16 ident;
	u16 fraginfo;	// 3 bits of flags; 13 bit fragment offset
	u08 ttl;
	u08 proto;
	u16 checksum;
	u32 src_addr;
	u32 dest_addr;
} ip_header;

// todo definitions for ip options

typedef enum IPPROTO
{
	IPPROTO_ICMP = 1,
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17,
} IPPROTO;

typedef struct arp_header
{
	u16 htype;
	u16 ptype;
	u08 hlen;
	u08 plen;
	u16 oper;
	mac_addr sha;
	u32 spa;
	mac_addr tha;
	u32 tpa;
} arp_header;

#define ARP_REQUEST	0x0001
#define ARP_REPLY	0x0002

typedef struct udp_header
{
	u16 src_port;
	u16 dest_port;
	u16 length;
	u16 checksum;
} udp_header;

enum tcp_flags
{
	TCP_FIN = 1,
	TCP_SYN = 2,
	TCP_RST = 4,
	TCP_PSH = 8,
	TCP_ACK = 16,
	TCP_URG = 32,
};

typedef struct tcp_header
{
	u16 src_port;
	u16 dest_port;
	u32 seq_no;
	u32 ack_no;
	u08 data_offset;
	u08 flags;
	u16 window;
	u16 checksum;
	u16 urgent_pointer;

	// todo: tcp options
} tcp_header;

typedef struct icmp_header
{
	u08 type;
	u08 code;
	u16 checksum;
	u16 id;
	u16 sequence;
} icmp_header;

typedef struct ip_pseudoheader		// for tcp and udp checksum
{
	u32 src;
	u32 dest;
	u08 sbz;
	u08 proto;
	u16 len;
} ip_pseudoheader;

#pragma pack( pop )

#endif