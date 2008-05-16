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

u32 get_bcastaddr( void );

u08 const * get_hostname( void );

__inline u08 is_in_subnet( u32 ip )
{
	u32 mask = get_netmask();
	return (get_hostaddr() & mask) == (ip & mask);
}

#endif