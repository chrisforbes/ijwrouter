#ifndef HTTPSERV_H
#define HTTPSERV_H

void httpserv_init( void );

#include "../str.h"

// for use in httpapp_dispatch_dynamic_request()

#define DISPATCH_ENDPOINT_V( endpoint_uri, endpoint_f )\
	if (strcmp( uri, endpoint_uri ) == 0)\
		{ endpoint_f( sock ); return 1; }
#define DISPATCH_ENDPOINT_S( endpoint_uri, endpoint_f )\
	if (strncmp( uri, endpoint_uri, sizeof(endpoint_uri) - 1 ) == 0)\
		{ endpoint_f( sock, uri + sizeof(endpoint_uri) - 1 ); return 1; }

void httpserv_send_content( tcp_sock sock, str_t mime_type, str_t content, u32 flags, u08 is_gzipped );
void httpserv_send_static_content( tcp_sock sock, str_t mime_type, str_t content, str_t _digest, u32 flags, u08 is_gzipped );
void httpserv_redirect( tcp_sock sock );	// todo: add uri to redirect to

// implemented by app:
u08 httpapp_dispatch_dynamic_request( tcp_sock sock, char const * uri );

#endif