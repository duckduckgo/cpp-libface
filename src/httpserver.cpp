#include <include/httpserver.hpp>
#include <include/utils.hpp>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#define CHECK(r, msg)                                           \
    if (r) {                                                    \
        uv_err_t err = uv_last_error(uv_loop);                  \
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(err));     \
        exit(1);                                                \
    }
#define UVERR(err, msg) fprintf(stderr, "%s: %s\n", msg, uv_strerror(err))

static const size_t MAX_URL_SIZE          = 2048;
static size_t MAX_OPEN_FDS                = 0;
static size_t MAX_CONNECTED_CLIENTS       = 0;    // Usually MAX_OPEN_FDS - 10

static uv_loop_t* uv_loop;                          // UV-event-loop pointer
static uv_tcp_t server;                             // Global TCP server
static http_parser_settings parser_settings;        // Global parser settings
static request_callback_t request_callback = NULL;  // The global request callback to invoke
static std::list<client_t*> connected_clients;      // The LRU list of connected clients. Front of the list is the least recently active client connection
static size_t nconnected_clients = 0;               // The # of currently connected clients
static std::list<client_t*> empty_list;             // Used to move nodes around in O(1) time by move_to_back()

enum {
    HTTP_PARSER_CONTINUE_PARSING = 0,
    HTTP_PARSER_STOP_PARSING     = 1
};

// Move element pointed to by 'iter' to the end of the list
// 'l'. 'iter' MUST be a member of 'l'.
void move_to_back(std::list<client_t*> &l, std::list<client_t*>::iterator iter) {
    assert(empty_list.empty());
    assert(!l.empty());
    empty_list.splice(empty_list.end(), l, iter);
    l.splice(l.end(), empty_list, empty_list.begin());
}

void build_HTTP_response_header(std::string &response_header,
                                int http_major, int http_minor,
                                int status_code, const char *status_str,
                                headers_t &headers,
                                std::string const &body) {
    response_header.clear();
    std::ostringstream os;
    char buff[2048];
    // Ensure that status_str is small enough that everything fits in under 2048 bytes.
    sprintf(buff, "HTTP/%d.%d %d %s\r\n", http_major, http_minor, status_code, status_str);
    os<<buff;
    sprintf(buff, "%u", body.size());
    headers["Content-Length"] = buff;
    for (headers_t::iterator i = headers.begin();
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
    const int http_major = client->parser.http_major;
    const int http_minor = client->parser.http_minor;
    if (http_should_keep_alive(&client->parser)) {
        headers["Connection"] = "Keep-Alive";
    } else {
        headers["Connection"] = "Close";
    }

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
    assert(client->cciter != connected_clients.end());
    connected_clients.erase(client->cciter);
    client->cciter = connected_clients.end();
    --nconnected_clients;
    uv_close((uv_handle_t*) &client->handle, on_close);
}

void on_close(uv_handle_t* handle) {
    DCERR("Connection Closed\n");
    client_t* client = (client_t*) handle->data;
    for (size_t i = 0; i < client->unparsed_data.size(); ++i) {
        free(client->unparsed_data[i].base);
    }
    // This is weird because handle is actually within 'client', so we
    // need to NULL out 'data' before we delete client.
    handle->data = NULL;
    delete client;
}

uv_buf_t on_alloc(uv_handle_t* client, size_t suggested_size) {
    // fprintf(stderr, "suggested_size: %d\n", suggested_size);
    uv_buf_t buf;
    buf.base = (char*)malloc(suggested_size);
    assert(buf.base);
    buf.len = suggested_size;
    return buf;
}

bool on_resume_read(client_t *client, partial_buf_t &pbuf) {
    ssize_t parsed;
    if (client->parser.http_errno == HPE_PAUSED) {
        return false;
    }

    ssize_t pending = pbuf.size - pbuf.offset;
    DPRINTF("# of bytes remaining: %d\n", pending);
    assert(pending > 0);

    parsed = http_parser_execute(&client->parser, &parser_settings, pbuf.base + pbuf.offset, pending);
    if (parsed < pending) {
        DPRINTF("parsed incomplete data::%d/%d bytes parsed\n", parsed, pending);
        if (client->parser.http_errno == HPE_PAUSED) {
            pbuf.offset += parsed;
        } else {
            close_connection(client);
        }
        return false;
    } else {
        return true;
    }
}

void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
    client_t* client = (client_t*) tcp->data;
    partial_buf_t pbuf(buf, nread, 0);
    assert(client->unparsed_data.empty());

    if (nread > 0) {
        bool consumed_all = on_resume_read(client, pbuf);
        if (!consumed_all) {
            buf.base = 0;
            client->unparsed_data.push_back(pbuf);
        }
    } else if (nread < 0) {
        // Always close the connection on error.
        // https://groups.google.com/forum/?fromgroups=#!topic/libuv/IG7tTbf6Zmg
        uv_err_t err = uv_last_error(uv_loop);
        if (err.code != UV_EOF) {
            UVERR(err, "read");
        }
        close_connection(client);
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

    if (status != 0 || !http_should_keep_alive(&client->parser)) {
        uv_err_t err = uv_last_error(uv_loop);
        UVERR(err, "write");
        close_connection(client);
        return;
    }

    // Free up all the buffers passed to uv_write().
    client->resstrs.clear();
    client->url.clear();

    // Resume parsing.
    http_parser_pause(&client->parser, 0);

    while (client->parser.http_errno != HPE_PAUSED &&
           !client->unparsed_data.empty()) {
        assert(client->unparsed_data.size() == 1);
        bool consumed_all = on_resume_read(client, client->unparsed_data.front());
        if (consumed_all) {
            free(client->unparsed_data[0].base);
            client->unparsed_data[0].base = NULL;
            client->unparsed_data.erase(client->unparsed_data.begin());
        }
    }

    assert(client->unparsed_data.size() <= 1);

    if (client->unparsed_data.empty()) {
        // Resume reading.
        uv_read_start(pstrm, on_alloc, on_read);
    }
}

void parse_query_string(std::string &qstr, query_strings_t &query) {
    std::string key, value;
    bool parsing_key = true;
    for (size_t i = 0; i < qstr.size(); ++i) {
        char ch = qstr[i];
        if (ch == '&') {
            query[key].swap(value);
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
        query[key].swap(value);
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
    if (client->url.size() > MAX_URL_SIZE) {
        // An obviously buggy request. The on_resume_read() function
        // will close the connection.
        DPRINTF("URL too long (%d > %d bytes)\n", client->url.size(), MAX_URL_SIZE);
        return HTTP_PARSER_STOP_PARSING;
    }
    return HTTP_PARSER_CONTINUE_PARSING;
}

int on_message_complete(http_parser* parser) {
    client_t* client = (client_t*) parser->data;

    DCERR("http message parsed\n");

    uv_stream_t *pstrm = (uv_stream_t*)(&client->handle);

    // Stop reading (to support request pipelining in HTTP/1.1).
    uv_read_stop(pstrm);

    // Pause request parsing.
    http_parser_pause(parser, 1);

    // Move this connection to the back of the LRU list (front being
    // the least recently accessed connection).
    move_to_back(connected_clients, client->cciter);

    // Invoke callback.
    request_callback(client);

    return HTTP_PARSER_CONTINUE_PARSING;
}

size_t get_max_open_fds() {
    struct rlimit rlim;
    int r = getrlimit(RLIMIT_NOFILE, &rlim);
    if (r != 0) {
        return 0;
    }
    return (size_t)rlim.rlim_cur;
}

int httpserver_start(request_callback_t rcb, const char *ip, int port) {
    int r;
    request_callback = rcb;

    parser_settings.on_message_complete = on_message_complete;
    parser_settings.on_url              = on_url;

    MAX_OPEN_FDS = get_max_open_fds();
    MAX_CONNECTED_CLIENTS = (MAX_OPEN_FDS > 10 ? MAX_OPEN_FDS - 10 : MAX_OPEN_FDS);

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

    // Ignore the SIGPIPE signal since we will handle it in-band.
    (void) signal(SIGPIPE, SIG_IGN);

    uv_run(uv_loop);
    return 0;
}
