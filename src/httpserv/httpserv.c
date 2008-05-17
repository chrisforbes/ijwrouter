#include <memory.h>
#include <string.h>

#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/tcp.h"
#include "../ip/arptab.h"
#include "../fs.h"
#include "../hal_debug.h"
#include "../user.h"
#include "httpserv.h"
#include "httpcommon.h"
#include "../str.h"
#include "../table.h"

#pragma warning( disable: 4204 )

// http pseudoheaders from request line
#define ph_method	((char const *)1)
#define ph_uri		((char const *)2)
#define ph_version	((char const *)3)

typedef void http_header_f( tcp_sock sock, char const * name, str_t const value );

#define MAXNAMESIZE		128
#define MAXVALUESIZE	128

#define __COPYINTO( ptr, base, value, max )\
	{ *ptr++ = value; if (ptr - base >= max) ptr = base; }

extern mac_addr str_to_mac( char const * buf );
extern char * mac_to_str( char * buf, void * mac );

static void httpserv_parse( tcp_sock sock, u08 const * data, u32 len, http_header_f * f )
{
	char name[ MAXNAMESIZE + 1 ], * pname = name;
	char value[ MAXVALUESIZE + 1 ], * pvalue = value;

	char const * p = (char const *) data;
	char const * end = p + len;

	u08 isval = 0;

	char const * isp = memchr( p, ' ', len );
	char const * irr = memchr( p, '\r', len );

	if (isp && irr && isp < irr)
	{
		char const * isp2 = memchr( isp + 1, ' ', len - ( isp - p ) );
		if (isp2 && isp2 < irr )
		{
			memcpy( value, p, isp - p );
			value[ isp - p ] = 0;
			f( sock, ph_method, __make_string( value, isp - p ) );

			memcpy( value, isp + 1, isp2 - isp - 1 );
			value[ isp2 - isp - 1 ] = 0;
			f( sock, ph_uri, __make_string( value, isp2 - isp - 1 ) );

			memcpy( value, isp2 + 1, irr - isp2 - 1 );
			value[ irr - isp2 - 1 ] = 0;
			f( sock, ph_version, __make_string( value, irr - isp2 - 1 ) );

			p = irr + 2;
		}
	}

	while( p < end )
	{
		char c = *p++;

		if ( !isval )
		{
			if ( c == '\n' )
				continue;

			if ( c == ':' )
			{
				*pname = 0;
				pvalue = value;
				isval = 1;
			}
			else __COPYINTO( pname, name, c, MAXNAMESIZE );
		}
		else
		{
			if ( pvalue == value && c == ' ' )
				continue;

			if ( c == '\r' )
			{
				*pvalue = 0;
				f( sock, name, __make_string( value, pvalue - value ) );
				pname = name;
				isval = 0;
			}
			else __COPYINTO( pvalue, value, c, MAXVALUESIZE );
		}
	}
}

static void httpserv_send_static_content( tcp_sock sock, str_t mime_type, str_t content, str_t _digest, u32 flags, u08 is_gzipped )
{
	str_t str = { malloc(512), 0 };	// bigger, to have space for the digest
	char mime[64];
	char digest[64];
	__memcpyz( mime, mime_type.str, mime_type.len );
	__memcpyz( digest, _digest.str, _digest.len );

	str.len = sprintf(str.str, 
		"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n%sContent-Type: %s\r\nETag: \"%s\"\r\n\r\n", 
		content.len, is_gzipped ? "Content-Encoding: gzip\r\n" : "", mime, digest);
	tcp_send( sock, str.str, str.len, 1 );
	tcp_send( sock, content.str, content.len, flags );
}

static void httpserv_send_content( tcp_sock sock, str_t mime_type, str_t content, u32 flags, u08 is_gzipped )
{
	str_t str = { malloc(256), 0 };
	char mime[64];
	__memcpyz( mime, mime_type.str, mime_type.len );

	str.len = sprintf(str.str, 
		"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n%sContent-Type: %s\r\nCache-Control: no-cache\r\nExpires: -1\r\n\r\n", 
		content.len, is_gzipped ? "Content-Encoding: gzip\r\n" : "", mime);
	tcp_send( sock, str.str, str.len, 1 );
	tcp_send( sock, content.str, content.len, flags );
}

static str_t httpserv_user_usage( user_t * u, u08 comma )
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

static str_t httpserv_get_usage_from_sock( tcp_sock sock )
{
	u32 host = tcp_gethost( sock );
	return httpserv_user_usage(get_user_by_ip(host), 0);
}

static void httpserv_send_all_usage( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );
	user_t * u = 0;

	while( (u = get_next_user(u)) != 0 )
	{
		str_t usage = httpserv_user_usage(u, get_next_user(u) != 0);
		append_string( &content, &usage );
		free( usage.str );
	}

	httpserv_send_content(sock, content_type, content, 1, 0 );
	logf( "200 OK %d bytes\n", content.len );
}

static str_t httpserv_user_binding( user_t * u, u08 comma )
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

			mac_to_str( mac, &m->eth_addr );
			mac[17] = ',';

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

static void httpserv_send_user_bindings( tcp_sock sock )
{
	str_t content = { 0, 0 };
	str_t content_type = MAKE_STRING( "application/x-json" );

	user_t * u = 0;

	while( 0 != (u = get_next_user(u) ))
	{
		str_t binding = httpserv_user_binding(u, get_next_user(u) != 0);
		append_string( &content, &binding );
		free( binding.str );
	}

	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

static void httpserv_send_error_status( tcp_sock sock, u32 status, str_t error_msg )
{
	str_t msg = { malloc(128), 0 };
	msg.len = sprintf(msg.str, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", 
		status, http_get_status_message(status), error_msg.len);
	tcp_send(sock, msg.str, msg.len, 1);
	tcp_send( sock, error_msg.str, error_msg.len, 0 );
}

static void httpserv_redirect( tcp_sock sock )
{
	str_t msg = MAKE_STRING("HTTP/1.1 302 Found\r\nLocation: /usage.htm\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
	tcp_send( sock, msg.str, msg.len, 0 );
	logf( "302 usage.htm\n" );
}

static void httpserv_set_name( tcp_sock sock, char const * name )
{
	user_t * u = get_user_by_ip(tcp_gethost(sock));
	if (u)
		strncpy(u->name, name, 32);
	httpserv_redirect( sock );	// send back to the usage page
}

static void httpserv_merge_mac( tcp_sock sock, char const * mac )
{
	mac_addr addr = str_to_mac( mac );
	user_t * u = get_user_by_ip( tcp_gethost(sock) );
	if (u)
		add_mac_to_user( u, addr );
	httpserv_redirect( sock );
}

static void httpserv_get_usage( tcp_sock sock )
{
	str_t content_type = MAKE_STRING( "application/x-json" );
	str_t content = httpserv_get_usage_from_sock(sock);
	httpserv_send_content(sock, content_type, content, 1, 0);
	logf( "200 OK %d bytes\n", content.len );
}

#define DISPATCH_ENDPOINT_V( endpoint_uri, endpoint_f )\
	if (strcmp( uri, endpoint_uri ) == 0)\
		{ endpoint_f( sock ); return; }
#define DISPATCH_ENDPOINT_S( endpoint_uri, endpoint_f )\
	if (strncmp( uri, endpoint_uri, sizeof(endpoint_uri) - 1 ) == 0)\
		{ endpoint_f( sock, uri + sizeof(endpoint_uri) - 1 ); return; }

static void httpserv_get_request( tcp_sock sock, str_t const _uri )
{
	struct file_entry const * entry;
	char const * uri = _uri.str;

	uri_decode((char *)uri, _uri.len, uri);		// dirty but actually safe (decoded uri is never longer, and
												// this buffer is always going to be in writable memory)
	logf( "GET %s : ", uri );

	if (! *uri++)
	{
		httpserv_send_error_status( sock, HTTP_STATUS_SERVER_ERROR, MAKE_STRING("Internal server error") );
		logf( "500 Internal Server Error\n" );
		return;
	}

	entry = ( *uri ) ? 
		fs_find_file( uri ) : fs_find_file( "index.htm" );
	
	if (!entry)
	{
		DISPATCH_ENDPOINT_V( "query/usage",		httpserv_get_usage );
		DISPATCH_ENDPOINT_V( "query/bindings",	httpserv_send_user_bindings );
		DISPATCH_ENDPOINT_V( "query/list",		httpserv_send_all_usage );
		DISPATCH_ENDPOINT_S( "name?name=",		httpserv_set_name );
		DISPATCH_ENDPOINT_S( "merge?",			httpserv_merge_mac );

		logf("404\n");
		httpserv_send_error_status(sock, HTTP_STATUS_NOT_FOUND, MAKE_STRING("Webpage could not be found."));
		return;
	}

	{
		httpserv_send_static_content(sock, 
			fs_get_str( &entry->content_type ), fs_get_str( &entry->content ), fs_get_str( &entry->digest ), 0, fs_is_gzipped( entry ));

		logf( "200 OK %d bytes\n", entry->content.length );
	}
}

static u08 current_method;

static void httpserv_header_handler( tcp_sock sock, char const * name, str_t const value )
{
	if (name == ph_method)
	{
		if (strcmp(value.str, "GET") == 0)
			current_method = http_method_get;
		else if (strcmp(value.str, "HEAD") == 0)
			current_method = http_method_head;
		else
		{
			current_method = http_method_other;
			httpserv_send_error_status(sock, HTTP_STATUS_NOT_IMPLEMENTED, MAKE_STRING("Method not implemented."));
		}

		return;
	}

	switch (current_method)
	{
	case http_method_get:
	case http_method_head:
		if (name == ph_uri)
			httpserv_get_request(sock, value);
	}
}

static void httpserv_handler( tcp_sock sock, tcp_event_e ev, void * data, u32 len, u32 flags )
{
	switch(ev)
	{
	case ev_opened:
		tcp_set_user_data( sock, 0 );
		break;
	case ev_closed:
		break;
	case ev_data:
		if (!tcp_get_user_data( sock ))
		{
			httpserv_parse( sock, (u08 const *)data, len, httpserv_header_handler);
			tcp_set_user_data( sock, (void *)1 );
		}
		break;
	case ev_releasebuf:
		if (flags)
			free(data);
		break;
	}
}

void httpserv_init( void )
{
	tcp_new_listen_sock(HTTP_PORT, httpserv_handler);
	logf("http: listening on port %d\n", HTTP_PORT);
}
