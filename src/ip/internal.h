#ifndef INTERNAL_H
#define INTERNAL_H

#include "../hal_debug.h"

void __send_packet( u08 iface, void const * buf, u16 len );

u16 __checksum_ex( u16 seed, void const * data, u16 size );
__inline u16 __checksum( void const * data, u16 size ) { return __checksum_ex( 0, data, size ); }
u16 __pseudoheader_checksum( ip_header const * ip );

__inline u16 __ip_header_length( ip_header const * h )
{
	return (h->version & 0x0f) << 2;		// ihl is in dwords
}

__inline void * __ip_payload( ip_header const * h )
{
	return ((u08 *)h) + __ip_header_length( h );
}

__inline u16 __ip_payload_length( ip_header const * h )
{
	return __ntohs( h->length ) - __ip_header_length( h );
}

__inline u08 __ip_validate_header( ip_header const * h )
{
	return 0xffff == __checksum( h, __ip_header_length( h ) );
}

__inline void * __tcp_payload( tcp_header* h )
{
	return 4 * h->data_offset + (u08*)h;
}

#endif