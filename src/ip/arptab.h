#ifndef ARPTAB_H
#define ARPTAB_H

void arptab_insert( u08 iface, u32 net_addr, mac_addr phys_addr );
u08 arptab_query( u08 * iface, u32 net_addr, mac_addr * phys_addr );
u08 arptab_queryif( u08 * iface, mac_addr * phys_addr );
void arptab_tick( void );

#endif