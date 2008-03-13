#ifndef TCP_H
#define TCP_H

typedef u16 tcp_sock;

u08 tcp_receive_packet( u08 iface, ip_header * p, u16 len );

typedef void tcp_listen_f( tcp_sock sock );
typedef void tcp_recv_f( tcp_sock sock, void* buf, u32 buf_len );

tcp_sock tcp_new_listen_sock( u16 port, tcp_listen_f* new_connection_callback, tcp_recv_f* recv_callback );

// buf should be malloc'd, and the tcp impl is responsible for free'ing it.
void tcp_send( tcp_sock sock, void* buf, u32 buf_len );

#endif
