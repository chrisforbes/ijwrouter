#include "../common.h"
#include "stack.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "../hal_debug.h"
#include "internal.h"

u08 tcp_receive_packet( u08 iface, ip_header * p, u16 len )
{
	iface; p; len;
	logf( "tcp: got packet\n" );
	return 0;
}
