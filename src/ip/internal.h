#ifndef INTERNAL_H
#define INTERNAL_H

#include "../hal_debug.h"

void __send_packet( u08 iface, u08 const * buf, u16 len );

__inline u16 __checksum( u16 const * base, u16 n )
{
	u32 x = 0;
	while( n-- )
		x += __ntohs(*base++);

	return ((u16)x) + ((u16)(x >> 16));
}

__inline u16 __ip_header_length( ip_header * h )
{
	return (h->version & 0x0f) << 2;		// ihl is in dwords
}

__inline void * __ip_payload( ip_header * h )
{
	return ((u08 *)h) + __ip_header_length( h );
}

__inline u16 __ip_payload_length( ip_header * h )
{
	return __ntohs( h->length ) - __ip_header_length( h );
}

__inline u08 __ip_validate_header( ip_header * h )
{
	return 0xffff == __checksum( (u16 const *)h, __ip_header_length( h ) >> 1 );
}

#endif