#ifndef ARP_H
#define ARP_H

u08 handle_arp_packet( u08 iface, arp_header * arp );
void send_arp_request( u08 iface, u32 ip );

#endif