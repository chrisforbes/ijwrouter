#include "common.h"
#include "ip/stack.h"
#include "ip/rfc.h"
#include "hal_ethernet.h"
#include "hal_debug.h"

char * mac_to_str( char * buf, void * _a )
{
	u08 const * a = _a;
	sprintf( buf, "%02x-%02x-%02x-%02x-%02x-%02x", 
		a[0], a[1], a[2],
		a[3], a[4], a[5] );
	return buf;
}

mac_addr str_to_mac( char const * buf )
{
	mac_addr result;
	u08 i = 0;
	u08 * p = result.addr;

	for( i = 0; i < 6; i++ )
	{
		*p++ = (u08)strtol( buf, 0, 16 );
		buf += 3;
	}
	return result;
}

void dump_packet( eth_packet * p )
{
	static char srcbuf[64], destbuf[64];

	mac_to_str( srcbuf, &p->packet->src );
	mac_to_str( destbuf, &p->packet->dest );

	logf( "src=%s@%02x dest=%s@%02x type=%04x len=%u\n", 
		srcbuf, p->src_iface, 
		destbuf, p->dest_iface, 
		__ntohs(p->packet->ethertype), p->len );
}
