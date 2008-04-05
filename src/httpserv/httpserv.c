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

// http pseudoheaders from request line
#define ph_method	((char const *)1)
#define ph_uri		((char const *)2)
#define ph_version	((char const *)3)

typedef void http_header_f( tcp_sock sock, char const * name, char const * value );

#define MAXNAMESIZE		128
#define MAXVALUESIZE	128

#define __COPYINTO( ptr, base, value, max )\
	{ *ptr++ = value; if (ptr - base >= max) ptr = base; }

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
			f( sock, ph_method, value );

			memcpy( value, isp + 1, isp2 - isp - 1 );
			value[ isp2 - isp - 1 ] = 0;
			f( sock, ph_uri, value );

			memcpy( value, isp2 + 1, irr - isp2 - 1 );
			value[ irr - isp2 - 1 ] = 0;
			f( sock, ph_version, value );

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
				f( sock, name, value );
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

static void httpserv_send_content( tcp_sock sock, char const * content_type, u32 content_type_len, char const * content, u32 content_len, u32 flags )
{
	char * msg = malloc( 128 );
	char mime[64];
	__memcpyz( mime, content_type, content_type_len );

	sprintf(msg, 
		"HTTP/1.0 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\nCache-Control: no-cache\r\nExpires: -1\r\n\r\n", 
		content_len, mime);
	tcp_send( sock, msg, strlen(msg), 1 );
	tcp_send( sock, content, content_len, flags );
}

static int hax = 0;

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

static char const * httpserv_user_usage( user * u, u08 comma )
{
	char * msg = malloc( 256 );
	char credit[16], quota[16];

	if (!u)
	{
		sprintf( msg, "{}" );
		return msg;
	}

	u->credit = u->quota ? ((u->credit + 2167425) % u->quota) : (u->credit + 2167425); //hack

	sprintf(msg, "{uname:\"%s\",start:\"%s\",current:\"%s\",quota:\"%s\",days:%d,fill:%d}%c",
		u->name,
		"1 January", 
		format_amount( credit, u->credit ),
		format_amount( quota, u->quota ),
		20, 
		u->quota ? (u->credit * 100 / u->quota) : 0,
		comma ? ',' : ' ');
	return msg;
}

static char const * httpserv_get_usage_from_sock( tcp_sock sock )
{
	u32 host = tcp_gethost( sock );
	return httpserv_user_usage(get_user_by_ip(host), 0);
}

static void httpserv_send_all_usage( tcp_sock sock )
{
	char const * usage;

	user * users;
	u32 num_users;
	u32 i;
	enumerate_users(&users, &num_users);

	for (i = 0; i < num_users; i++)
	{
		usage = httpserv_user_usage(users, i == num_users - 1);
		httpserv_send_content(sock, "application/x-json", 18, usage, strlen(usage), 1);
		users += sizeof(user);
	}
}

static void httpserv_send_error_status( tcp_sock sock, u32 status, char const * error_msg )
{
	char msg[128];
	u32 errorlen = strlen( error_msg );
	sprintf(msg, "HTTP/1.0 %d %s\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", 
		status, http_get_status_message(status), errorlen);
	tcp_send(sock, msg, strlen(msg), 0);
	tcp_send( sock, error_msg, errorlen, 0 );
}

static void httpserv_get_request( tcp_sock sock, char const * uri )
{
	struct file_entry const * entry;
	char const * content;
	char const * content_type;

	if (! *uri++)
	{
		httpserv_send_error_status( sock, HTTP_STATUS_SERVER_ERROR, "Internal server error" );
		return;
	}

	entry = ( *uri ) ? 
		fs_find_file( uri ) : fs_find_file( "index.htm" );
	
	if (!entry)
	{
		if (strcmp(uri, "usage") == 0)
		{
			content = httpserv_get_usage_from_sock(sock);
			httpserv_send_content(sock, "application/x-json", 18, content, strlen(content), 1);
		}
		if (strcmp(uri, "list") == 0)
		{
			httpserv_send_all_usage(sock);
		}
		else
			httpserv_send_error_status(sock, HTTP_STATUS_NOT_FOUND, "Webpage could not be found.");
		return;
	}

	content_type = fs_get_mimetype(entry);
	content = fs_get_content(entry);

	httpserv_send_content(sock, content_type, entry->mime_pair.length, content, entry->content_pair.length, 0);
}

static u08 current_method;

static void httpserv_header_handler( tcp_sock sock, char const * name, char const * value )
{
	if (name == ph_method)
	{
		if (strcmp(value, "GET") == 0)
		{
			current_method = http_method_get;
		}
		else if (strcmp(value, "HEAD") == 0)
		{
			current_method = http_method_head;
		}
		else
		{
			current_method = http_method_other;
			httpserv_send_error_status(sock, HTTP_STATUS_NOT_IMPLEMENTED, "Method not implemented.");
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