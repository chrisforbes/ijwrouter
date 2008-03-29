#ifndef TCP_H
#define TCP_H

typedef u16 tcp_sock;

#define INVALID_TCP_SOCK	0xffff

u08 tcp_receive_packet( ip_header * p, u16 len );

typedef enum tcp_event_e
{
	ev_opened,
	ev_closed,
	ev_data,
	ev_releasebuf,
} tcp_event_e;

typedef void tcp_event_f( tcp_sock sock, tcp_event_e ev, void * data, u32 len );

tcp_sock tcp_new_listen_sock( u16 port, tcp_event_f * handler );

// buffer will be passed back in an ev_releasebuf event when we're done.
void tcp_send( tcp_sock sock, void* buf, u32 buf_len );

#endif
