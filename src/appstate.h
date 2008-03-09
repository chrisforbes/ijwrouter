#ifndef APPSTATE_H
#define APPSTATE_H

typedef struct ip_state
{
	u08 foo;
} ip_state;

typedef ip_state uip_tcp_appstate_t;

void ip_appcall(void);

#define UIP_APPCALL ip_appcall

#endif