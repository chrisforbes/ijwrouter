#ifndef UDP_H
#define UDP_H
typedef struct udp_header
{
	u16 sourcePort;
	u16 destPort;
	u16 length;
	u16 checksum;
}
#endif