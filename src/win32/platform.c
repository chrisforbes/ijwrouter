#include "../common.h"

u16 __htons( u16 x ) { return ((u08)x << 8) | (u08)(x >> 8); }
u16 __ntohs( u16 x ) { return __htons(x); }

u32 __htonl( u32 x ) { return (__htons((u16)x) << 16) | (__htons(x >> 16)); }
u32 __ntohl( u32 x ) { return __htonl(x); }