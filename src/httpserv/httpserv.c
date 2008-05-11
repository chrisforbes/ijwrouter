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
	// zero-terminating memcpy
static void __inline __memcpyz( char * dest, char const * src, u32 len )
{
	memcpy( dest, src, len );
	dest[len] = 0;
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

char * format_amount( char * buf, u64 x )
{
	static char const * units[] = { "KB", "MB", "GB", "TB" };
	char const ** u = units;

	while( x > (1 << 20) * 9 / 10 )
	{
		x >>= 10;
		u++;
	}

	sprintf( buf, "%1.2f %s", x / 1024.0f, *u );
	return buf;
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

	user_t * u = get_next_user(0);

	while( u )
	{
		str_t usage = httpserv_user_usage(u, get_next_user(u) != 0);
		u32 old_len = content.len;
		grow_string( &content, usage.len );
		memcpy( content.str + old_len, usage.str, usage.len );
		free( usage.str );
		u = get_next_user(u);
	}

	httpserv_send_content(sock, content_type, content, 1, 0 );
	logf( "200 OK %d bytes\n", content.len );
}

static str_t httpserv_user_binding( user_t * u, u08 comma )
{
	str_t str = { malloc(256), 0 };
	str_t macs = { 0, 0 };
	mac_mapping_t * m = get_next_mac(0);

	if (!u)
		return MAKE_STRING("{}");

	while (m)
	{
		if (m->user == u)
		{
			char mac[18];
			u32 old_len = macs.len;
			mac_to_str( mac, &m->eth_addr );
			mac[17] = ',';
			grow_string( &macs, 18 );
			memcpy( macs.str + old_len, mac, 18 );
		}
		m = get_next_mac(m);
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

	user_t * u = get_next_user(0);

	while( u )
	{
		str_t binding = httpserv_user_binding(u, get_next_user(u) != 0);
		u32 old_len = content.len;
		grow_string( &content, binding.len );
		memcpy( content.str + old_len, binding.str, binding.len );
		free( binding.str );
		u = get_next_user(u);
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

static u08 decode_hex( char c )
{
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= '0' && c <= '9')
		return c - '0';
	logf("char %c is not a valid hex digit\n", c);
	return 0;
}

static void uri_decode( char * dest, u32 len, char const * src )
{
	u08 c, s = 0;
	while (0 != (c = *src++) && len--)
	{
		switch (s)
		{
		case 0:
			if (c == '%') s = 1;
			else *dest++ = c;
			break;
		case 1:
			*dest = decode_hex( c ) << 4;
			s = 2;
			break;
		case 2:
			*dest++ |= decode_hex( c );
			s = 0;
			break;
		}
	}
	*dest = 0;
}

static void httpserv_redirect( tcp_sock sock )
{
	str_t msg = MAKE_STRING("HTTP/1.1 302 Found\r\nLocation: /usage.htm\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
	tcp_send( sock, msg.str, msg.len, 0 );
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
		if (strcmp(uri, "usage") == 0)
		{
			str_t content_type = MAKE_STRING( "application/x-json" );
			str_t content = httpserv_get_usage_from_sock(sock);
			httpserv_send_content(sock, content_type, content, 1, 0);
			logf( "200 OK %d bytes\n", content.len );
		}
		else if (strcmp(uri, "query/bindings") == 0)
			httpserv_send_user_bindings(sock);
		else if (strcmp(uri, "list") == 0)
			httpserv_send_all_usage(sock);
		else if (strncmp(uri, "name?", 5) == 0)
			httpserv_set_name(sock, uri + 5);
		else if (strncmp(uri, "merge?", 6) == 0)
			httpserv_merge_mac(sock, uri + 6);
		else
		{
			logf("404\n");
			httpserv_send_error_status(sock, HTTP_STATUS_NOT_FOUND, MAKE_STRING("Webpage could not be found."));
		}
		return;
	}

	{
		str_t content_type = { (char *)fs_get_mimetype( entry ), entry->mime_pair.length };
		str_t content = { (char *)fs_get_content( entry ), entry->content_pair.length };

		httpserv_send_content(sock, content_type, content, 0, fs_is_gzipped( entry ));
		logf( "200 OK %d bytes\n", content.len );
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
