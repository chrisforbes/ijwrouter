#include "common.h"
#include "hal_debug.h"
#include "hal_time.h"
#include "ip/rfc.h"
#include "ip/stack.h"
#include "ip/tcp.h"
#include "user.h"
#include "httpserv/httpserv.h"
#include "httpserv/httpcommon.h"
#include "stats.h"
#include "str.h"
#include "billing.h"

extern mac_addr str_to_mac( char const * buf );
extern char * mac_to_str( char * buf, void * mac );

static str_t httpapp_user_usage( user_t * u, u08 comma )
{
	str_t str = { malloc( 256 ), 0 };

	if (!u)
		return MAKE_STRING("{}");

	str.len = sprintf(str.str, "{uname:\"%s\",current:%I64u,flags:%u}%c",
		u->name, u->credit, u->flags, comma ? ',' : ' ');
	return str;
}

static str_t httpapp_get_usage_from_sock( tcp_sock sock )
{
	return httpapp_user_usage(get_user_by_ip(tcp_gethost( sock )), 0);
}

static void httpapp_send_all_usage( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );
	user_t * u = 0;

	while( (u = get_next_user(u)) != 0 )
	{
		str_t usage = httpapp_user_usage(u, get_next_user(u) != 0);
		append_string( &content, &usage );
		free( usage.str );
	}

	httpserv_send_content(sock, content_type, content, 1, 0 );
	logf( "200 OK %d bytes\n", content.len );
}

static void httpapp_set_name( tcp_sock sock, char const * name )
{
	user_t * u = get_user_by_ip(tcp_gethost(sock));
	if (u) 
	{
		strncpy(u->name, name, 32);
		u->flags |= USER_CUSTOM_NAME;
	}
	httpserv_redirect_to( sock, "usage.htm" );
}

static void httpapp_merge_mac( tcp_sock sock, char const * mac )
{
	user_t * u = get_user_by_ip( tcp_gethost(sock) );
	user_t * v = get_user_by_name( mac );
	if (u && v)
		merge_users(u, v);
	httpserv_redirect_to( sock, "usage.htm" );
}

static void httpapp_get_usage( tcp_sock sock )
{
	str_t content_type = MAKE_STRING( "application/x-json" );
	str_t content = httpapp_get_usage_from_sock(sock);
	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

static str_t httpapp_user_binding( user_t * u, u08 comma )
{
	str_t str = { malloc(256), 0 };
	str_t macs = { 0, 0 };
	mac_mapping_t * m = 0;

	if (!u)
		return MAKE_STRING("{}");

	while (0 != (m = get_next_mac(m)))
	{
		if (m->user == u)
		{
			char mac[18];
			str_t mac_str = __make_string( mac, 18 );
			mac_to_str( mac, &m->eth_addr ); mac[17] = ',';
			append_string( &macs, &mac_str );
		}
	}

	macs.str[macs.len - 1] = 0;

	str.len = sprintf(str.str, "{uname: \"%s\",macs: \"%s\"}%c",
		u->name,
		macs.str,
		comma ? ',' : ' ');

	return str;
}

static void httpapp_send_user_bindings( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );

	user_t * u = 0;

	while( 0 != (u = get_next_user(u) ))
	{
		str_t binding = httpapp_user_binding(u, get_next_user(u) != 0);
		append_string( &content, &binding );
		free( binding.str );
	}

	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

static str_t httpapp_make_counter_json(void * counter, u08 comma)
{
	str_t str = { malloc(128), 0 };

	if (!counter) return MAKE_STRING("{}");

	str.len = sprintf(str.str, "{counter_name: \"%s\",count: %I64u}%c",
		stats_get_counter_name(counter),
		stats_get_counter_count(counter),
		comma ? ',' : ' ');

	return str;
}

static void httpapp_send_stat_counts( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );

	void * c = 0;
	while( 0 != (c = stats_get_next_counter(c) ))
	{
		str_t counter = httpapp_make_counter_json(c, stats_get_next_counter(c) != 0);
		append_string( &content, &counter );
		free( counter.str );
	}

	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

static void httpapp_force_commit( tcp_sock sock )
{	// forces changes to account db to be committed immediately
	save_users();
	httpserv_redirect_to( sock, "usage.htm" );
}

static void httpapp_handle_new_user( tcp_sock sock )
{
	user_t * u = get_user_by_ip( tcp_gethost( sock ) );
	httpserv_redirect_to( sock, 
		(u->flags & USER_CUSTOM_NAME) ? "usage.htm" : "new.htm");
}

static void httpapp_set_billing_period( tcp_sock sock, char const * day )
{
	set_rollover_day( (u08)atoi(day) );
	logf("Set billing period rollover day to %s\n", day);
	httpserv_redirect_to( sock, "list.htm" );
}

static void httpapp_get_billing_info( tcp_sock sock )
{
	str_t content = { malloc(128), 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );

	content.len = sprintf( content.str, 
		"{start: %u, end: %u, now: %u, day: %d}", 
		get_start_of_period(),
		get_end_of_period(),
		get_time(),
		get_rollover_day() );

	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

static void httpapp_get_csv( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "text/csv" );

	user_t * u = 0;

	str_t a = MAKE_STRING( "name,current_usage,last_usage\n" );
	append_string( &content, &a );
	
	while( 0 != ( u = get_next_user(u) ) )
	{
		char sz[128];
		str_t temp = { 0, 0 };
		temp.str = sz;
		temp.len = sprintf( sz, "%s,%I64u,%I64u\n",
			u->name, u->credit, u->last_credit );
		append_string( &content, &temp );
	}

	httpserv_send_content( sock, content_type, content, 1, 0 );
	logf( "200 OK %d bytes" );
}

u08 httpapp_dispatch_dynamic_request( tcp_sock sock, char const * uri )
{
	DISPATCH_ENDPOINT_V( "",				httpapp_handle_new_user );
	DISPATCH_ENDPOINT_V( "query/usage",		httpapp_get_usage );
	DISPATCH_ENDPOINT_V( "query/bindings",	httpapp_send_user_bindings );
	DISPATCH_ENDPOINT_V( "query/list",		httpapp_send_all_usage );
	DISPATCH_ENDPOINT_V( "query/stats",		httpapp_send_stat_counts );
	DISPATCH_ENDPOINT_V( "query/billing",	httpapp_get_billing_info );
	DISPATCH_ENDPOINT_S( "name?name=",		httpapp_set_name );
	DISPATCH_ENDPOINT_S( "merge?name=",		httpapp_merge_mac );
	DISPATCH_ENDPOINT_S( "period?day=",		httpapp_set_billing_period );
	DISPATCH_ENDPOINT_V( "commit",			httpapp_force_commit );
	DISPATCH_ENDPOINT_V( "usage.csv",		httpapp_get_csv );
	return 0;
}