#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <sstream>
#include "libuv/include/uv.h"
#include "http-parser/http_parser.h"

typedef std::map<std::string, std::string> headers_t;
typedef std::map<std::string, std::string> query_strings_t;

struct partial_buf_t : uv_buf_t {
    size_t size;
    size_t offset;

    partial_buf_t(uv_buf_t buf, size_t s, size_t o)
        : uv_buf_t(buf), size(s), offset(o) { }
};

struct client_t {
    uv_tcp_t                       handle;
    http_parser                    parser;
    uv_write_t                     write_req;
    std::vector<std::string>       resstrs;
    std::vector<partial_buf_t>     unparsed_data;
    std::list<client_t*>::iterator cciter;
    std::string                    url;

    client_t() { }
};

struct parsed_url_t {
    std::string path;
    query_strings_t query;
};

typedef void (*request_callback_t)(client_t*);

void build_HTTP_response_header(std::string &response_header,
                                int http_major, int http_minor,
                                int status_code, const char *status_str,
                                headers_t &headers,
                                std::string const &body);
void write_response(client_t *client,
                    int status_code,
                    const char *status_str,
                    headers_t &headers,
                    std::string &body);
void on_close(uv_handle_t* handle);
uv_buf_t on_alloc(uv_handle_t* client, size_t suggested_size);
void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf);
void on_connect(uv_stream_t* server_handle, int status);
void after_write(uv_write_t* req, int status);
void parse_query_string(std::string &qstr, query_strings_t &query);
void parse_URL(std::string const &url_str, parsed_url_t &uout);
int on_url(http_parser *parser, const char *data, size_t len);
int on_message_complete(http_parser* parser);
int httpserver_start(request_callback_t rcb, const char *ip, int port);

#endif // HTTPSERVER_HPP
