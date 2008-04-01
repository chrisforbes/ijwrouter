#include <memory.h>
#include <string.h>

#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/tcp.h"
#include "../fs.h"
#include "httpserv.h"
#include "httpcommon.h"

typedef void http_header_f( tcp_sock sock, char const * name, char const * value );

#define MAXNAMESIZE		128
#define MAXVALUESIZE	128

#define __COPYINTO( ptr, base, value, max )\
	{ *ptr++ = value; if (ptr - base >= max) ptr = base; }

void httpserv_parse( tcp_sock sock, u08 const * data, u32 len, http_header_f * f )
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
			f( sock, "Method", value );

			memcpy( value, isp + 1, isp2 - isp - 1 );
			value[ isp2 - isp - 1 ] = 0;
			f( sock, "Uri", value );

			memcpy( value, isp2 + 1, irr - isp2 - 1 );
			value[ irr - isp2 - 1 ] = 0;
			f( sock, "Version", value );

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

void httpserv_send_content( tcp_sock sock, char const * content_type, u32 content_type_len, char const * content, u32 content_len )
{
	char msg[128];
	//avert thine eyes, this is about to get ugly
	char mime[64];
	memcpy(mime, content_type, content_type_len);
	mime[content_type_len] = 0;

	//ok it's safe to look again
	sprintf(msg, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", content_len, mime);
	tcp_send( sock, msg, strlen(msg) );
	tcp_send( sock, content, content_len );
}

void httpserv_send_error_status( tcp_sock sock, u32 status, char const * error_msg )
{
	char msg[128];
	u32 errorlen = strlen( error_msg );
	sprintf(msg, "HTTP/1.0 %d %s\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", 
		status, http_get_status_message(status), errorlen);
	tcp_send(sock, msg, strlen(msg));
	tcp_send( sock, error_msg, errorlen );
}

void httpserv_get_request( tcp_sock sock, char const * uri, u32 uri_length )
{
	struct file_entry const * entry;
	char const * content;
	char const * content_type;

	if (*uri == '/' && uri_length == 1)
	{
		entry = fs_find_file("index.htm");
	}
	else
	{
		uri++;
		entry = fs_find_file(uri);
	}
	
	if (!entry)
	{
		httpserv_send_error_status(sock, HTTP_STATUS_NOT_FOUND, "Webpage could not be found.");
		return;
	}

	content_type = fs_get_mimetype(entry);
	content = fs_get_content(entry);

	httpserv_send_content(sock, content_type, entry->mime_pair.length, content, entry->content_pair.length);
}

static u08 current_method;

void httpserv_header_handler( tcp_sock sock, char const * name, char const * value )
{
	if (strcmp(name, "Method") == 0)
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
		if (strcmp(name, "Uri") == 0)
			httpserv_get_request(sock, value, strlen(value));
		break;
	case http_method_head:
		if (strcmp(name, "Uri") == 0)
			httpserv_get_request(sock, value, strlen(value));
		break;
	}
}

extern void logf( char const * str, ... );

void httpserv_handler( tcp_sock sock, tcp_event_e ev, void * data, u32 len )
{
	switch(ev)
	{
	case ev_opened:
		logf("Got HTTP connection\n");
		break;
	case ev_closed:
		logf("HTTP Connection closed\n");
		break;
	case ev_data:
		httpserv_parse( sock, (u08 const *)data, len, httpserv_header_handler);
		break;
	}
}

void httpserv_init( void )
{
	tcp_new_listen_sock(HTTP_PORT, httpserv_handler);
	logf("http: listening on port %d\n", HTTP_PORT);
}