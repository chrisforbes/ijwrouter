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

void __send_packet( u08 iface, void const * buf, u16 len )
{
	if (on_send)
		on_send( iface, (u08 const *)buf, len );
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

static u08 ip_receive_packet( ip_header * p, u16 len )
{
	logf( "ip: got ip packet, proto=%d\n", p->proto );

	if (!__ip_validate_header( p ))
	{
		logf( "ip: bad ip checksum\n" );
		return 1;
	}

	switch( p->proto )
	{
	case IPPROTO_ICMP: return icmp_receive_packet( p, len );
	case IPPROTO_UDP: return udp_receive_packet( p, len );
	case IPPROTO_TCP: return tcp_receive_packet( p, len );
	}

	return 1;		// did we eat it? yes we did...
}

static u08 __ip_receive_packet( ip_header * p, u16 len )	// did we eat it?
{
	// check if it's actually for us!
	if (p->dest_addr != get_bcastaddr() && 
		p->dest_addr != get_hostaddr() && p->dest_addr != 0xfffffffful)
		return 0;

	return ip_receive_packet( p, len ) 
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
			return __ip_receive_packet( ip, len - sizeof( eth_header ) );
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

u16 __pseudoheader_checksum( ip_header const * ip )
{
	ip_pseudoheader ipph;
	ipph.src = ip->src_addr;
	ipph.dest = ip->dest_addr;
	ipph.proto = ip->proto;
	ipph.len = __htons(__ip_payload_length( ip ));
	ipph.sbz = 0;

	return __checksum_ex( 0, &ipph, sizeof(ipph) );
}

void __ip_make_header( ip_header * ip, u08 proto, u16 ident, u16 len, u32 dest )
{
	ip->version = 0x45;
	ip->tos = 0;
	ip->length = __htons( len );
	ip->fraginfo = 0;
	ip->ident = ident;
	ip->dest_addr = dest;
	ip->src_addr = get_hostaddr();
	ip->ttl = 128;
	ip->proto = proto;
	ip->checksum = ~__htons( __checksum( ip, sizeof(ip_header) ) );
}

void __ip_make_response( ip_header * ip, ip_header const * req, u16 len )
{
	__ip_make_header( ip, req->proto, req->ident, len, req->src_addr );
}
