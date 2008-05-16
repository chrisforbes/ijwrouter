// ijw router win32 stub

#pragma comment( lib, "ws2_32.lib" )
#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/conf.h"
#include "../hal_ethernet.h"
#include "../hal_debug.h"
#include "../user.h"
#include "../hal_time.h"

#include "../ip/stack.h"
#include "../ip/tcp.h"

#include "../httpserv/httpserv.h"
#include "../fs.h"

#include <assert.h>

u08 charge_for_packet( eth_packet * p )
{
	mac_addr lanside = (p->dest_iface == IFACE_WAN) 
		? p->packet->dest : p->packet->src;

	user_t * u = get_user( lanside );

	if (!u)
	{
		logf("- (dropped, no user)\n");
		return eth_discard( p );
	}

	if (u->credit >= p->len)
	{
		u->credit -= p->len;
		logf("+ (ok)\n");
		return eth_forward( p );
	}
	
	logf("- (dropped, insufficient funds)\n");
	return eth_discard( p );	// do not want
}

void eth_inject_packet( u08 iface, u08 const * data, u16 len )
{
	eth_packet p;
	p.src_iface = IFACE_INTERNAL;
	p.dest_iface = iface;
	p.len = len;
	p.packet = (eth_header *)data;

	eth_inject( &p );
}

u08 handle_packet( eth_packet * p )
{
	u16 ethertype = __ntohs(p->packet->ethertype);

	// throw away packets from us (yes, we see them with winpcap!)
	if (mac_equal( p->packet->src, get_macaddr()))
		return eth_discard( p );

//	dump_packet( p );

	if (p->dest_iface == IFACE_INTERNAL || p->dest_iface == IFACE_BROADCAST)
	{
	//	logf( "packet: internal or bcast\n" );
		return ipstack_receive_packet( p->src_iface, (u08 const *)p->packet, p->len ) 
			? eth_discard( p ) : eth_forward( p );
	}

	if (ethertype != ethertype_ipv4 )
	{
	//	logf( "- non-ip (ethertype=%x)\n", ethertype );
		return eth_discard( p );
	}

	if (p->src_iface == p->dest_iface)
	{
	//	logf( "- self-route\n" );
		return eth_discard( p );
	}

	//logf( "+ pure lan\n" );
	return eth_forward( p );	// not crossing from lan <-> wan	*/
}

extern void dhcp_init( void );
extern void dhcp_process( void );

extern void nbns_process( void );
extern void nbns_init( void );

int main( void )
{
	u08 interfaces = eth_init();

	ipstack_init( eth_inject_packet );

	if (!interfaces)
		logf( "! no interfaces available\n" );

	dhcp_init();
	nbns_init();

	fs_init();

	httpserv_init();

	for(;;)
	{
		eth_packet p;
		if (eth_getpacket( &p ))
			handle_packet( &p );
		dhcp_process();
		nbns_process();
	}
}