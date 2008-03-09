#ifndef USER_H
#define USER_H

typedef struct user
{
	char name[16];
	u64 credit;
} user;

user * get_user( uip_eth_addr const * addr );

#endif