#include "common.h"
#include "appstate.h"

#include <uip/uip.h>

void tcp_appcall(void) {}

void udp_appcall(void) 
{
	if (uip_udp_conn->appstate.handler)
		uip_udp_conn->appstate.handler();
}

void bind_handler( void * conn, packet_handler_f * handler )
{
	((struct uip_udp_conn *)conn)->appstate.handler = handler;
}