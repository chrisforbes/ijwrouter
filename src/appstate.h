#ifndef APPSTATE_H
#define APPSTATE_H

typedef struct tcp_state
{
	u08 foo;
} tcp_state;

typedef tcp_state uip_tcp_appstate_t;

typedef struct udp_state
{
	u08 foo;
} udp_state;

typedef udp_state uip_udp_appstate_t;

void tcp_appcall(void);

#define UIP_APPCALL tcp_appcall
#define UIP_UDP_APPCALL udp_appcall

#endif