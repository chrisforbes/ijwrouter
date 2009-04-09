#include "../common.h"
#include "../ip/rfc.h"
#include "../ip/tcp.h"
#include "../fs.h"
#include "../hal_debug.h"
#include "httpserv.h"
#include "httpcommon.h"

#pragma warning( disable: 4204 )

// http pseudoheaders from request line
#define ph_method	((char const *)1)
#define ph_uri		((char const *)2)
#define ph_version	((char const *)3)

typedef void http_header_f( tcp_sock sock, char const * name, str_t const value );

typedef enum http_state_e 
{ 
	s_init,
	s_method, s_uri, s_version, 
	s_name, s_value, s_done 
} http_state_e;

typedef struct http_state_t
{
	http_state_e state;
	char n[128], v[128];
	char * p;
} http_state_t;

#define TRANSITION_YIELDING_PH( c, newstate, ph )\
	if (ch == c)\
	{\
		*(s->p) = 0;\
		f( sock, ph, __make_string2( s->n, s->p ));\
		s->state = newstate;\
		s->p = s->n;\
		return;\
	}

#define OTHERWISE_APPEND()\
	*(s->p)++ = ch;\
	return

static void httpserv_parse2_ch( tcp_sock sock, http_state_t * s, http_header_f * f, char ch )
{
	switch( s->state )
	{
	case s_init:
		s->state = s_method;
		s->p = s->n;
		// fall through

	case s_method: 
		TRANSITION_YIELDING_PH( ' ', s_uri, ph_method );
		OTHERWISE_APPEND();

	case s_uri:
		TRANSITION_YIELDING_PH( ' ', s_version, ph_uri );
		OTHERWISE_APPEND();

	case s_version:
		TRANSITION_YIELDING_PH( '\r', s_name, ph_version );
		OTHERWISE_APPEND();		

	case s_name:
		if (ch == '\n') return;
		if (ch == '\r') { s->state = s_done; return; }
		if (ch == ':')
		{
			s->state = s_value;
			*(s->p) = 0;	// ensure null-terminated
			s->p = s->v;
			return;
		}
		OTHERWISE_APPEND();

	case s_value:
		if (ch == ' ' && s->p == s->v) return;
		if (ch == '\r')
		{
			s->state = s_name;
			*(s->p) = 0;	// ensure null-terminated
			f( sock, s->n, __make_string2( s->v, s->p ) );
			s->p = s->n;
			return;
		}
		if (s->p - s->v >= 126) return;
		OTHERWISE_APPEND();
	}
}

static void httpserv_parse2( tcp_sock sock, http_state_t * s, 
					   u08 const * data, u32 len, http_header_f * f )
{
	u08 const * p = data;
	while( len-- && s->state != s_done )
		httpserv_parse2_ch( sock, s, f, *p++ );
}

void httpserv_send_static_content( tcp_sock sock, str_t mime_type, str_t content, str_t _digest, u32 flags, u08 is_gzipped )
{
	str_t str = { malloc(512), 0 };
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

static void httpserv_send_401( tcp_sock sock )
{
	str_t str = { malloc(512), 0 };
	const char* pageHtml = "<html><head><title>401</title></head><body><h1>401 Authorization Required</h1></body></html>";
	size_t len = strlen( pageHtml );

	str.len = sprintf(str.str, 
		"HTTP/1.1 401 Authorization Required\r\n"
		"Connection: close\r\n"
		"WWW-Authenticate: Basic realm=\"IJW Router\"\r\n" // <---- adjust realm
		"Content-Length: %d\r\n"
		"\r\n", 
		len );
	tcp_send( sock, str.str, str.len, 1 );
	tcp_send( sock, pageHtml, len, 0 );
}

void httpserv_send_content( tcp_sock sock, str_t mime_type, str_t content, u32 flags, u08 is_gzipped )
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

static void httpserv_send_error_status( tcp_sock sock, u32 status, str_t error_msg )
{
	str_t msg = { malloc(128), 0 };
	msg.len = sprintf(msg.str, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", 
		status, http_get_status_message(status), error_msg.len);
	tcp_send(sock, msg.str, msg.len, 1);
	if (error_msg.len)
		tcp_send( sock, error_msg.str, error_msg.len, 0 );
}

void httpserv_redirect_to( tcp_sock sock, char const * uri )
{
	str_t msg = { malloc(512), 0 };
	msg.len = sprintf(msg.str, "HTTP/1.1 302 Found\r\nLocation: /%s\r\n"
		"Content-Length: 0\r\nConnection: close\r\n\r\n", uri);
	tcp_send( sock, msg.str, msg.len, 1 );
	log_printf( "302 %s\n", uri );
}

typedef struct http_request_t
{
	http_state_t parser_state;
	u08 _uri[128];
	str_t uri;
	u08 method;
	u08 digest[32];
	u08 has_digest;
	u08 has_authorization;
} http_request_t;

static void httpserv_get_request( tcp_sock sock, str_t const _uri, http_request_t * req )
{
	struct file_entry const * entry;
	char const * uri = _uri.str;

	uri_decode((char *)uri, _uri.len, uri);		// dirty but actually safe (decoded uri is never longer, and
												// this buffer is always going to be in writable memory)
	log_printf( "GET %s : ", uri );

	if (! *uri++)
	{
		httpserv_send_error_status( sock, HTTP_STATUS_SERVER_ERROR, MAKE_STRING("Internal server error") );
		log_printf( "500 Internal Server Error\n" );
		return;
	}

	if( !req->has_authorization )
	{
		httpserv_send_401( sock );
		log_printf( "401\n" );
		return;
	}

	entry = fs_find_file( uri );
	
	if (!entry)
	{
		if (httpapp_dispatch_dynamic_request( sock, uri ))
			return;

		log_printf("404\n");
		httpserv_send_error_status(sock, HTTP_STATUS_NOT_FOUND, MAKE_STRING("Webpage could not be found."));
		return;
	}

	if ( req->has_digest && 0 == memcmp( fs_get_str( &entry->digest ).str, req->digest, 32 ) )
	{
		// the client already has this resource
		httpserv_send_error_status( sock, HTTP_STATUS_NOT_MODIFIED, MAKE_STRING("") );
		log_printf( "304 Not Modified\n" );
		return;
	}

	httpserv_send_static_content(sock, 
		fs_get_str( &entry->content_type ), 
		fs_get_str( &entry->content ), 
		fs_get_str( &entry->digest ), 0, fs_is_gzipped( entry ));

	log_printf( "200 OK %d bytes\n", entry->content.length );
}

static u08 http_check_user( const char* user, const char* password )
{
	if( strcmp( user, "admin" ) == 0 && strcmp( password, "admin" ) == 0 )
		return 1;
	return 0;
}

static u32 base64_char_to_num( char c )
{
	if( c >= 'A' && c <= 'Z' )
		return c - 'A';
	if( c >= 'a' && c <= 'z' )
		return 26 + c - 'a';
	if( c >= '0' && c <= '9' )
		return 52 + c - '0';
	if( c == '+' )
		return 62;
	if( c == '/' )
		return 63;
	if( c == '=' )
		return 0;
	return (u32)-1;
}

static void httpserv_header_handler( tcp_sock sock, char const * name, str_t const value )
{
	http_request_t * req = (http_request_t *)tcp_get_user_data( sock );

	if (name == ph_method)
	{
		if (strcmp(value.str, "GET") == 0) req->method = http_method_get;
		else if (strcmp(value.str, "HEAD") == 0) req->method = http_method_head;
		else
		{
			req->method = http_method_other;
			httpserv_send_error_status(sock, HTTP_STATUS_NOT_IMPLEMENTED, MAKE_STRING("Method not implemented."));
		}

		return;
	}

	if (name == ph_uri)
	{
		memcpy( req->_uri, value.str, value.len );
		req->uri = __make_string( (char *)req->_uri, value.len );
		return;
	}

	if (name > ph_version)
	{
		if (strcmp( name, "If-None-Match" ) == 0)
		{
			// grab the digest, if it exists
			memcpy( req->digest, value.str + 1, 32 );
			req->has_digest = 1;
		}
		else if( strcmp( name, "Authorization" ) == 0 )
		{
			if( value.len >= 6 && memcmp( value.str, "Basic ", 6 ) == 0 )
			{
				size_t len = value.len - 6;
				const char* encoded = value.str + 6;

				char decoded[128];
				char* decoded_next = decoded;
				while( len >= 4 && decoded_next < decoded+126 )
				{
					u32 c, d, e, f;
					c = base64_char_to_num( *encoded++ ); if( c == -1 ) break;
					d = base64_char_to_num( *encoded++ ); if( d == -1 ) break;
					e = base64_char_to_num( *encoded++ ); if( e == -1 ) break;
					f = base64_char_to_num( *encoded++ ); if( f == -1 ) break;
					c = c << 6 | d;
					c = c << 6 | e;
					c = c << 6 | f;

					*decoded_next++ = (c>>16) & 0xff;
					*decoded_next++ = (c>>8) & 0xff;
					*decoded_next++ = c & 0xff;

					len -= 4;
				}
				*decoded_next = 0; // null terminate

				for( decoded_next = decoded ; *decoded_next != 0 ; decoded_next++ )
					if( *decoded_next == ':' )
					{
						*decoded_next = 0;
						if( http_check_user( decoded, decoded_next + 1 ) )
							req->has_authorization = 1;
						break;
					}
			}
			//else if( value starts with "Digest " ....
		}
	}
}

static void httpserv_handler( tcp_sock sock, tcp_event_e ev, void * data, u32 len, u32 flags )
{
	switch(ev)
	{
	case ev_opened:
		{
			http_request_t * req = malloc( sizeof( http_request_t ) );
			memset( req, 0, sizeof(req) );
			tcp_set_user_data( sock, req );
		}
		break;
	case ev_closed:
		{
			void* x = tcp_get_user_data( sock );
			if (x) free(x);
			tcp_set_user_data( sock, 0 );
			break;
		}
	case ev_data:
		{
			http_request_t * req = tcp_get_user_data( sock );
			memset( req, 0, sizeof( http_request_t ) );
			httpserv_parse2( sock, &req->parser_state,
				(u08 const *)data, len, httpserv_header_handler);

			if (req->parser_state.state == s_done && 
				(req->method == http_method_get || req->method == http_method_head))
				httpserv_get_request( sock, req->uri, req );
		}
		break;
	case ev_releasebuf:
		if (flags) free(data);
		break;
	}
}

void httpserv_init( void )
{
	tcp_new_listen_sock(HTTP_PORT, httpserv_handler);
	log_printf("http: listening on port %d\n", HTTP_PORT);
}
