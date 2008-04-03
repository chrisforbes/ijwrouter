#ifndef USER_H
#define USER_H

typedef struct user
{
	char name[16];
	u64 credit;
	u64 quota;
} user;

user * get_user( mac_addr addr );

#endif