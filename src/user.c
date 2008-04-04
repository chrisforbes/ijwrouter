#include "common.h"
#include "user.h"
#include "ip/conf.h"

#include <winsock2.h>	// temp

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
	{ "cheshire", 0, 0 },
	{ "rob", 0, 0 },
};

user * get_user_by_ip( u32 addr )
{
	if (addr == inet_addr( "192.168.2.4" ))
		return &users[0];

	if (addr == inet_addr( "192.168.2.8" ))
		return &users[1];

	if (addr == inet_addr( "192.168.2.2" ))
		return &users[2];

	if (addr == inet_addr( "192.168.2.3" ))
		return &users[3];

	return 0;
}