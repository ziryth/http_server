#ifndef HTTP_H
#define HTTP_H

#include "base/base_inc.h"

enum HttpMethod {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_PATCH,
};

enum HttpVersion {
    HTTP_VERSION_10,
    HTTP_VERSION_11,
};
typedef struct HttpHeader HttpHeader;
struct HttpHeader {
    String8 key;
    String8 value;
    HttpHeader *next;
};

typedef struct HttpRequest HttpRequest;
struct HttpRequest {
    HttpMethod method;
    String8 path;
    HttpVersion version;
    HttpHeader *headers;
    String8 body;
};

typedef struct HttpResponse HttpResponse;
struct HttpResponse {
    HttpHeader *headers;
    String8 body;
};

HttpRequest http_parse_request(String8 request_buffer);
void http_parse_method(HttpRequest *request, String8 method_line);
void http_parse_headers(HttpRequest *request, String8 header_string);

#endif // HTTP_H
