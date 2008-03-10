#ifndef APPSTATE_H
#define APPSTATE_H

typedef struct tcp_state
{
	u08 foo;
} tcp_state;

typedef tcp_state uip_tcp_appstate_t;

typedef void packet_handler_f (void);

typedef struct udp_state
{
	packet_handler_f * handler;
} udp_state;

typedef udp_state uip_udp_appstate_t;

void tcp_appcall(void);
void udp_appcall(void);

#define UIP_APPCALL tcp_appcall
#define UIP_UDP_APPCALL udp_appcall

void bind_handler( void * conn, packet_handler_f * handler );

#endif
