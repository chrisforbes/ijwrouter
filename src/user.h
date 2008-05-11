#ifndef USER_H
#define USER_H

typedef struct user_t
{
	char name[32];
	u64 credit;
	u64 quota;
	u08 references;
	u08 flags;
} user_t;

typedef struct mac_mapping_t
{
	mac_addr eth_addr;
	user_t * user;
} mac_mapping_t;

user_t * get_user( mac_addr addr );
user_t * get_user_by_ip( u32 addr );

user_t * get_next_user( user_t * );
mac_mapping_t * get_next_mac ( mac_mapping_t * m );
void add_mac_to_user( user_t *, mac_addr );

#endif