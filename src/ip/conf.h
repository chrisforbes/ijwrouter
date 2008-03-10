#ifndef CONF_H
#define CONF_H

/*
	Host network configuration
*/

u32 get_hostaddr( void );
u32 get_netmask( void );
mac_addr get_macaddr( void );

void set_hostaddr( u32 x );
void set_netmask( u32 x );
void set_macaddr( mac_addr x );

#endif