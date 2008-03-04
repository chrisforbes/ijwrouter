#ifndef TCP_H
#define TCP_H
//As defined by RFC 793
typedef struct tcp_header
{
	u16 sourcePort;
	u16 destPort;
	u32 sequenceNum;
	u32 ackNum;
	u16 dataOffset:4;
	u16 reserved:6;
	u16 urg:1;
	u16 ack:1;
	u16 psh:1;
	u16 rst:1;
	u16 syn:1;
	u16 fin:1;
	u16 window;
	u16 checksum;
	u16 urgentPointer;
};
#endif