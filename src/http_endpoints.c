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

extern mac_addr str_to_mac( char const * buf );
extern char * mac_to_str( char * buf, void * mac );

static str_t httpapp_user_usage( user_t * u, u08 comma )
{
	char credit[16], quota[16];
	str_t str = { malloc( 256 ), 0 };

	if (!u)
		return MAKE_STRING("{}");

	u->credit = u->quota ? ((u->credit + 2167425) % u->quota) : (u->credit + 2167425); //hack

	str.len = sprintf(str.str, "{uname:\"%s\",start:\"%s\",current:\"%s\",quota:\"%s\",days:%d,fill:%d}%c",
		u->name,
		"1 January", 
		format_amount( credit, u->credit ),
		format_amount( quota, u->quota ),
		20,
		(u08)(u->quota ? (u->credit * 100 / u->quota) : 0),
		comma ? ',' : ' ');
	return str;
}

static str_t httpapp_get_usage_from_sock( tcp_sock sock )
{
	u32 host = tcp_gethost( sock );
	return httpapp_user_usage(get_user_by_ip(host), 0);
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
		strncpy(u->name, name, 32);
	httpserv_redirect( sock );	// send back to the usage page
}

static void httpapp_merge_mac( tcp_sock sock, char const * mac )
{
	mac_addr addr = str_to_mac( mac );
	user_t * u = get_user_by_ip( tcp_gethost(sock) );
	if (u)
		add_mac_to_user( u, addr );
	httpserv_redirect( sock );
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

u08 httpapp_dispatch_dynamic_request( tcp_sock sock, char const * uri )
{
	DISPATCH_ENDPOINT_V( "query/usage",		httpapp_get_usage );
	DISPATCH_ENDPOINT_V( "query/bindings",	httpapp_send_user_bindings );
	DISPATCH_ENDPOINT_V( "query/list",		httpapp_send_all_usage );
	DISPATCH_ENDPOINT_V( "query/stats",		httpapp_send_stat_counts );
	DISPATCH_ENDPOINT_S( "name?name=",		httpapp_set_name );
	DISPATCH_ENDPOINT_S( "merge?",			httpapp_merge_mac );
	return 0;
}