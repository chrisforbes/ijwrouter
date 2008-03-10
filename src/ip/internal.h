#ifndef INTERNAL_H
#define INTERNAL_H

void __send_packet( u08 iface, u08 const * buf, u16 len );

__inline u16 __checksum( u16 const * base, u16 n )
{
	u32 x = 0;
	while( n-- )
		x += __ntohs(*base++);

	return ((u16)x) + ((u16)(x >> 16));
}

#endif