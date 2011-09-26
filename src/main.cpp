
// C-headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"

// Custom-includes
#include <include/segtree.hpp>
#include <include/phrase_map.hpp>


// C++-headers
#include <string>


std::string
get_qs(const struct mg_request_info *request_info, std::string const& key) {
    char val[1024];
    std::string rv;

    val[1023] = val[0] = '\0';
    // printf("query string: %s\n", request_info->query_string);

    int val_len = 0;
    if (request_info->query_string) {
        val_len = mg_get_var(request_info->query_string, 
                             strlen(request_info->query_string), 
                             key.c_str(), val, 1023);
        rv = val;
    }
    return rv;
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");
    std::string file = get_qs(request_info, "file");


    return "";
}

static void*
handle_suggest(enum mg_event event,
               struct mg_connection *conn,
               const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");
    std::string q = get_qs(request_info, "q");

    return "";
}

static void*
handle_stats(enum mg_event event,
             struct mg_connection *conn,
             const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");

    return "";
}

static void*
handle_invalid_request(enum mg_event event,
                       struct mg_connection *conn,
                       const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 404 Not Found\r\n"
              "Content-Type: text/plain\r\n\r\n");
    return "";
}



static void*
callback(enum mg_event event,
         struct mg_connection *conn,
         const struct mg_request_info *request_info) {
    if (event == MG_NEW_REQUEST) {
        std::string request_uri = request_info->uri;
        if (request_uri == "/face/suggest/") {
            retrurn handle_suggest(event, conn, request_info);
        }
        else if (request_uri == "/face/import/") {
            retrurn handle_import(event, conn, request_info);
        }
        else if (request_uri == "/face/stats/") {
            retrurn handle_stats(event, conn, request_info);
        }
        else {
            retrurn handle_invalid_request(event, conn, request_info);
        }
    }

    // Echo requested URI back to the client

    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");
    char q[1024];
    q[1023] = '\0';
    // printf("query string: %s\n", request_info->query_string);

    int ql = 0;
    if (request_info->query_string) {
      ql = mg_get_var(request_info->query_string, 
		      strlen(request_info->query_string), 
		      "q", q, 1023);
    }

    // printf("%p, %d\n", q, ql);

    mg_printf(conn, "q: %s, ql: %d", q ? q : "", ql);
    return empty_string;  // Mark as processed
  } else {
    return NULL;
  }
}

int main(void) {
  struct mg_context *ctx;
  const char *options[] = {"listening_ports", "6767", NULL};

  ctx = mg_start(&callback, NULL, options);
  while (1) {
    // Never stop
    sleep(100);
  }
  mg_stop(ctx);

  return 0;
}
