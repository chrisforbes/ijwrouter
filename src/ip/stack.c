#include "../common.h"
#include "stack.h"
#include "arptab.h"
#include "rfc.h"
#include "conf.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "../hal_debug.h"
#include "internal.h"

static send_packet_f * on_send = 0;

void __send_packet( u08 iface, u08 const * buf, u16 len )
{
	if (on_send)
		on_send( iface, buf, len );
	else
		logf( "ip: trying to send packet but no driver bound.\n" );
}

void ipstack_init( send_packet_f * send_callback )
{
	on_send = send_callback;
	// todo: initialize some other things
}

void ipstack_tick( void )
{
	arptab_tick();
}

static u08 ip_receive_packet( u08 iface, ip_header * p, u16 len )
{
	logf( "ip: got ip packet, proto=%d\n", p->proto );

	if (!__ip_validate_header( p ))
	{
		logf( "ip: bad ip checksum\n" );
		return 1;
	}

	switch( p->proto )
	{
	case IPPROTO_ICMP:
		return icmp_receive_packet( iface, p, len );

	case IPPROTO_UDP:
		return udp_receive_packet( iface, p, len );

	case IPPROTO_TCP:
		return tcp_receive_packet( iface, p, len );
	}

	return 1;		// did we eat it? yes we did...
}

static u08 __ip_receive_packet( u08 iface, ip_header * p, u16 len )	// did we eat it?
{
	if (p->dest_addr != get_bcastaddr() && p->dest_addr != get_hostaddr())
		return 0;

	return ip_receive_packet( iface, p, len ) 
		&& p->dest_addr != get_bcastaddr();
}

u08 ipstack_receive_packet( u08 iface, u08 const * buf, u16 len )
{
	eth_header * eth = (eth_header *) buf;
	u16 etype = __ntohs( eth->ethertype ); 

	len;

	switch( etype )
	{
	case ethertype_ipv4:
		{
			ip_header * ip = (ip_header *) (eth + 1);
			arptab_insert( iface, ip->src_addr, eth->src );
			return __ip_receive_packet( iface, ip, len - sizeof( eth_header ) );
		}

	case ethertype_arp:
			return handle_arp_packet( iface, (arp_header *) (eth + 1) );
	}

	return 0;
}

u16 __checksum_ex( u16 sum, void const * _data, u16 len )
{
	u16 t;
	const u08 * data = _data;
	const u08 *dataptr;
	const u08 *last_byte;

	dataptr = data;
	last_byte = data + len - 1;

	while(dataptr < last_byte) {	/* At least two more bytes */
		t = (dataptr[0] << 8) + dataptr[1];
		sum += t;
		if(sum < t) ++sum;	// carry
		dataptr += 2;
	}

	if(dataptr == last_byte) {
		t = (dataptr[0] << 8) + 0;
		sum += t;
		if(sum < t) ++sum;	// carry
	}

	/* Return sum in host byte order. */
	return sum;
}
