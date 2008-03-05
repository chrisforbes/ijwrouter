#ifndef USER_H
#define USER_H

typedef struct user
{
	char name[16];
	u64 credit;
} user;

user * get_user( mac_address const * addr );

#endif