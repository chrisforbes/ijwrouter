#ifndef USER_H
#define USER_H

typedef struct user_t
{
	char name[32];
	u64 credit;
	u64 last_credit;
	u64 quota;
	u08 references;
	u08 flags;
} user_t;

#define USER_NEW	0x01
#define USER_CUSTOM_NAME	0x02

typedef struct mac_mapping_t
{
	mac_addr eth_addr;
	user_t * user;
} mac_mapping_t;

user_t * get_user( mac_addr addr );
user_t * get_user_by_ip( u32 addr );
user_t * get_user_by_name( char const * name );

user_t * get_next_user( user_t * );
mac_mapping_t * get_next_mac ( mac_mapping_t * m );
void merge_users( user_t * from, user_t * to );

void save_users( void );
void restore_users( void );
void do_periodic_save( void );

#endif