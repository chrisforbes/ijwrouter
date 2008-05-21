#ifndef HTTPCOMMON_H
#define HTTPCOMMON_H

#define HTTP_PORT ((u08)80)

#define HTTP_STATUS_OK				200
#define HTTP_STATUS_NOT_MODIFIED	304
#define HTTP_STATUS_NOT_FOUND		404
#define HTTP_STATUS_SERVER_ERROR	500
#define HTTP_STATUS_NOT_IMPLEMENTED	501

typedef enum http_method_e
{
	http_method_none,
	http_method_get,
	http_method_head,
	http_method_other,
} http_method_e;

__inline char const * http_get_status_message(u32 status)
{
	switch(status)
	{
	case HTTP_STATUS_OK:				return "OK";
	case HTTP_STATUS_NOT_FOUND:			return "Not Found";
	case HTTP_STATUS_SERVER_ERROR:		return "Internal Server Error";
	case HTTP_STATUS_NOT_IMPLEMENTED:	return "Not Implemented";
	case HTTP_STATUS_NOT_MODIFIED:		return "Not Modified";
	}
	return "";
}

#endif