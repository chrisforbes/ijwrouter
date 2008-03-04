#ifndef IP_H
#define IP_H
//As defined by RFC 791
typedef struct ip_header
{
	u08 version:4;
	u08 ihl:4;
	u08 serviceType;
	u16 totalLength;
	u16 id;
	u16 flags:3;
	u16 fragOffset:13;
	u08 ttl;
	u08 protocol;
	u16 checksum;
	u32 sourceAddr;
	u32 destAddr;
};
#endif