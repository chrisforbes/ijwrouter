#ifndef COMMON_H
#define COMMON_H

/*
	defines core types & macros
*/

typedef unsigned char u08;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef signed char i08;
typedef signed short i16;
typedef signed long i32;
typedef signed long long i64;

#include <stdlib.h>
#include <stdio.h>
//#include <memory.h>
#include <string.h>

u16 __htons( u16 x );
u16 __ntohs( u16 x );

u32 __htonl( u32 x );
u32 __ntohl( u32 x );

typedef struct mac_addr { u08 addr[6]; } mac_addr;

__inline u08 mac_equal( mac_addr a, mac_addr b )
{
	return 0 == memcmp( &a, &b, sizeof(a) );
}

__inline u32 make_ip( u08 a, u08 b, u08 c, u08 d )
{
	u08 x[4];
	x[0] = a; x[1] = b; x[2] = c; x[3] = d;
	return *( u32 * ) &x;
}

#endif
