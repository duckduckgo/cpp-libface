#include <include/httpserver.hpp>
#include <include/utils.hpp>

#define CHECK(r, msg)                                           \
    if (r) {                                                    \
        uv_err_t err = uv_last_error(uv_loop);                  \
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(err));     \
        exit(1);                                                \
    }
#define UVERR(err, msg) fprintf(stderr, "%s: %s\n", msg, uv_strerror(err))

static uv_loop_t* uv_loop;
static uv_tcp_t server;
static http_parser_settings parser_settings;
static request_callback_t request_callback = NULL;
static std::list<client_t*> connected_clients;
static const int MAX_CONNECTED_CLIENTS = 900;
static int nconnected_clients = 0;

void build_HTTP_response_header(std::string &response_header,
                                int http_major, int http_minor,
                                int status_code, const char *status_str,
                                headers_t &headers,
                                std::string const &body) {
    response_header.clear();
    std::ostringstream os;
    char buff[2048];
    sprintf(buff, "HTTP/%d.%d %d %s\r\n", http_major, http_minor, status_code, status_str);
    os<<buff;
    sprintf(buff, "%d", body.size());
    headers["Content-Length"] = buff;
    headers["Connection"]     = "Keep-alive";
    for (headers_t::const_iterator i = headers.begin();
         i != headers.end(); ++i) {
        os<<i->first<<": "<<i->second<<"\r\n";
    }
    os<<"\r\n";
    response_header = os.str();
}

void write_response(client_t *client,
                    int status_code,
                    const char *status_str,
                    headers_t &headers,
                    std::string &body) {
    assert(client->resstrs.empty());
    std::string header_str;
    int http_major = 1, http_minor = 1;
    build_HTTP_response_header(header_str, http_major, http_minor,
                               status_code, status_str, headers, body);

    client->resstrs.resize(2);
    client->resstrs[0].swap(header_str);
    client->resstrs[1].swap(body);

    uv_buf_t resbuf[2];
    resbuf[0].base = (char*)client->resstrs[0].c_str();
    resbuf[0].len  = client->resstrs[0].size();

    resbuf[1].base = (char*)client->resstrs[1].c_str();
    resbuf[1].len  = client->resstrs[1].size();

    uv_write(&client->write_req, (uv_stream_t*)&client->handle,
             resbuf, 2, after_write);
}

void close_connection(client_t *client) {
    connected_clients.erase(client->cciter);
    client->cciter = connected_clients.end();
    --nconnected_clients;
    uv_close((uv_handle_t*) &client->handle, on_close);
}

void on_close(uv_handle_t* handle) {
    DCERR("Connection Closed\n");
    client_t* client = (client_t*) handle->data;
    delete client;
}

uv_buf_t on_alloc(uv_handle_t* client, size_t suggested_size) {
    uv_buf_t buf;
    buf.base = (char*)malloc(suggested_size);
    assert(buf.base);
    buf.len = suggested_size;
    return buf;
}

void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
    ssize_t parsed;
    client_t* client = (client_t*) tcp->data;

    if (nread > 0) {
        parsed = http_parser_execute(&client->parser, &parser_settings, buf.base, nread);
        if (parsed < nread) {
            DPRINTF("parse error::nread:%d, parsed: %d\n", nread, parsed);
            close_connection(client);
        }
    } else {
        uv_err_t err = uv_last_error(uv_loop);
        if (err.code != UV_EOF) {
            UVERR(err, "read");
            close_connection(client);
        }
    }

    free(buf.base);
}

void on_connect(uv_stream_t* server_handle, int status) {
    if (status != 0) {
        uv_err_t err = uv_last_error(uv_loop);
        UVERR(err, "connect");
        return;
    }
    assert((uv_tcp_t*)server_handle == &server);

    int r;
    client_t* client = new client_t;

    ++nconnected_clients;
    DCERR("New Connection::nconnected_clients: " << nconnected_clients << "\n");

    uv_tcp_init(uv_loop, &client->handle);
    http_parser_init(&client->parser, HTTP_REQUEST);

    client->parser.data = client;
    client->handle.data = client;
    client->cciter = connected_clients.insert(connected_clients.end(), client);

    if (nconnected_clients > MAX_CONNECTED_CLIENTS) {
        // Close the oldest connected.
        DCERR("Calling close_connection() on first socket\n");
        close_connection(connected_clients.front());
    }

    r = uv_accept(server_handle, (uv_stream_t*)&client->handle);
    CHECK(r, "accept");

    uv_stream_t *pstrm = (uv_stream_t*)&client->handle;
    uv_read_start(pstrm, on_alloc, on_read);
}

void after_write(uv_write_t* req, int status) {
    client_t *client = (client_t*)(req->handle->data);
    uv_stream_t *pstrm = (uv_stream_t*)(&client->handle);

    if (status != 0) {
        uv_err_t err = uv_last_error(uv_loop);
        UVERR(err, "write");
        close_connection(client);
        return;
    }

    // Free up all the buffers passed to uv_write().
    client->resstrs.clear();
    client->url.clear();

    http_parser_init(&client->parser, HTTP_REQUEST);
    client->parser.data = client;

    uv_read_start(pstrm, on_alloc, on_read);
}


void parse_query_string(std::string &qstr, query_strings_t &query) {
    std::string key, value;
    bool parsing_key = true;
    for (size_t i = 0; i < qstr.size(); ++i) {
        char ch = qstr[i];
        if (ch == '&') {
            query[key] = value;
            key.clear();
            value.clear();
            parsing_key = true;
        } else if (ch == '=') {
            parsing_key = false;
        } else {
            if (parsing_key) {
                key += ch;
            } else {
                value += ch;
            }
        }
    }
    if (!key.empty()) {
        query[key] = value;
    }
}

void parse_URL(std::string const &url_str, parsed_url_t &uout) {
    struct http_parser_url url;
    http_parser_parse_url(url_str.c_str(), url_str.size(), 0, &url);

    if (url.field_set & (1<<UF_PATH)) {
        int foff = url.field_data[UF_PATH].off;
        int flen = url.field_data[UF_PATH].len;
        uout.path.assign(url_str.c_str() + foff, flen);
    }

    if (url.field_set & (1<<UF_QUERY)) {
        int foff = url.field_data[UF_QUERY].off;
        int flen = url.field_data[UF_QUERY].len;
        std::string qstr(url_str.c_str() + foff, flen);
        parse_query_string(qstr, uout.query);
    }
}

int on_url(http_parser *parser, const char *data, size_t len) {
    client_t* client = (client_t*) parser->data;
    DPRINTF("Adding '%s' to URL\n", std::string(data, len).c_str());
    client->url.append(data, len);
    DPRINTF("URL is now: %s\n", client->url.c_str());
    return 0;
}

int on_message_complete(http_parser* parser) {
    client_t* client = (client_t*) parser->data;
  
    DCERR("http message parsed\n");

    uv_stream_t *pstrm = (uv_stream_t*)(&client->handle);

    uv_read_stop(pstrm);

    // Invoke callback.
    request_callback(client);
    return 1;
}

int httpserver_start(request_callback_t rcb, const char *ip, int port) {
    int r;
    request_callback = rcb;

    parser_settings.on_message_complete = on_message_complete;
    parser_settings.on_url              = on_url;

    uv_loop = uv_default_loop();
    r = uv_tcp_init(uv_loop, &server);
    if (r != 0) {
        return r;
    }

    struct sockaddr_in address = uv_ip4_addr(ip, port);
    r = uv_tcp_bind(&server, address);
    if (r != 0) {
        return r;
    }
    uv_listen((uv_stream_t*)&server, 128, on_connect);

    uv_run(uv_loop);
    return 0;
}
