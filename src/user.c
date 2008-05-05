#include "common.h"
#include "user.h"
#include "ip/conf.h"

extern u32 __stdcall inet_addr (char const * cp);

static u08 is_in_subnet( u32 ip )
{
	u32 mask = get_netmask();
	return (get_hostaddr() & mask) == (ip & mask);
}

user * get_user( mac_addr addr )
{
	addr;	// todo: map the address to the user
	return 0;
}

user users[] = 
{
	{ "odd-socks", 0, 0 },
	{ "other-woman", 104857600u, 1073741824u },
	{ "laptop-0", 0, 104857600u },
	{ "laptop-1", 0, 104857600u },
};

user * get_user_by_ip( u32 addr )
{
	if (addr == inet_addr( "192.168.2.4" ))
		return &users[0];

	if (addr == inet_addr( "192.168.2.8" ))
		return &users[1];

	if (addr == inet_addr( "192.168.2.9" ))
		return &users[2];

	if (addr == inet_addr( "192.168.0.105" ))
		return &users[3];

	return 0;
}

void enumerate_users( user ** u, u32 * num_users )
{
	*u = users;
	*num_users = 4;
}