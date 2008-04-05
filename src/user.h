#ifndef USER_H
#define USER_H

typedef struct user
{
	char name[16];
	u64 credit;
	u64 quota;
} user;

user * get_user( mac_addr addr );
user * get_user_by_ip( u32 addr );

void enumerate_users( user ** u, u32 * num_users );

#endif