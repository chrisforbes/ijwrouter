#ifndef USER_H
#define USER_H

typedef struct user_t
{
	char name[32];
	u64 credit;
	u64 quota;
} user_t;

user_t * get_user( mac_addr addr );
user_t * get_user_by_ip( u32 addr );

user_t * get_next_user( user_t * );

#endif