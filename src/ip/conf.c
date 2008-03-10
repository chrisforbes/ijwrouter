#include "../common.h"
#include "conf.h"
#include "../hal_debug.h"

static u32 __hostaddr = 0;
static u32 __netmask = 0xfffffffful;
static mac_addr __macaddr = { { 0x00, 0x19, 0xe0, 0xff, 0x09, 0x08 } };

u32 get_hostaddr( void ) { return __hostaddr; }
u32 get_netmask( void ) { return __netmask; }
mac_addr get_macaddr( void ) { return __macaddr; }

void set_hostaddr( u32 x )
{ 
	__hostaddr = x; 
	logf( "conf: host address = %u.%u.%u.%u\n", 
		(u08)(x >> 0), (u08)(x >> 8), (u08)(x >> 16), (u08)(x >> 24) );
}
void set_netmask( u32 x )
{ 
	__netmask = x; 
	logf( "conf: netmask = %u.%u.%u.%u\n", 
		(u08)(x >> 0), (u08)(x >> 8), (u08)(x >> 16), (u08)(x >> 24) );
}

void set_macaddr( mac_addr x ) { __macaddr = x; }

u32 get_bcastaddr( void )
{
	return (__hostaddr & __netmask) | ~__netmask;
}