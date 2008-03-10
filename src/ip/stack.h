#ifndef STACK_H
#define STACK_H

typedef u08 send_packet_f( u08 iface, u08 const * buf, u16 len );	// callback to send an ethernet frame

void ipstack_init( send_packet_f * send_callback );						// called by the app/driver at startup
void ipstack_tick( void );												// called by the app/driver every so often
void ipstack_receive_packet( u08 iface, u08 const * buf, u16 len );		// called by the driver when a packet is received

#endif