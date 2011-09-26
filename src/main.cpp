
// C-headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"

// Custom-includes
#include <include/segtree.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>

// C++-headers
#include <string>
#include <fstream>

PhraseMap pm;
SegmentTree st;
bool building = false;

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

void
print_HTTP_response(struct mg_connection *conn, 
                    int code, const char *description, 
                    const char *content_type = "text/plain") {
    mg_printf(conn, "HTTP/1.1 %d %s\r\n"
              "Content-Type: %s\r\n\r\n", code, description, content_type);
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
    std::ifstream fin(file.c_str());

    if (!fin) {
        print_HTTP_response(conn, 404, "Not Found");
    }
    else {
        building = true;
        pm.repr.clear();
        char buff[4096];

        while (fin) {
            fin.getline(buff, 4096);
            int llen = fin.gcount();
            buff[4095] = '\0';
            int tabpos = std::find(buff, buff + llen, '\t') - buff;
            if (tabpos < llen && tabpos > 0 && tabpos < llen - 1) {
                std::string phrase(buff, tabpos);
                uint_t weight = atoi(buff + tabpos + 1);
                pm.insert(phrase, weight);
            }
        }
        pm.finalize();
        vui_t weights;
        for (size_t i = 0; i < pm.repr.size(); ++i) {
            weights.push_back(pm.repr[i].second);
        }
        st.initialize(weights);

        print_HTTP_response(conn, 200, "OK");
        mg_printf(conn, "Successfully added %d records from \"%s\"", weights.size(), file.c_str());

        building = false;
    }

    return (void*)"";
}

static void*
handle_suggest(enum mg_event event,
               struct mg_connection *conn,
               const struct mg_request_info *request_info) {
    print_HTTP_response(conn, 200, "OK");

    std::string q  = get_qs(request_info, "q");
    std::string sn = get_qs(request_info, "n");
    int n = sn.empty() ? 16 : atoi(sn.c_str());
    if (n < 0 || n > 16) {
        n = 16;
    }

    vpsui_t results = suggest(pm, st, q, n);

    for (size_t i = 0; i < results.size(); ++i) {
        mg_printf(conn, "%s:%d\n", results[i].first.c_str(), results[i].second);
    }

    return (void*)"";
}

static void*
handle_stats(enum mg_event event,
             struct mg_connection *conn,
             const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");

    return (void*)"";
}

static void*
handle_invalid_request(enum mg_event event,
                       struct mg_connection *conn,
                       const struct mg_request_info *request_info) {
    mg_printf(conn, "HTTP/1.1 404 Not Found\r\n"
              "Content-Type: text/plain\r\n\r\n");

    return (void*)"";
}



static void*
callback(enum mg_event event,
         struct mg_connection *conn,
         const struct mg_request_info *request_info) {
    if (event == MG_NEW_REQUEST) {
        std::string request_uri = request_info->uri;
        if (request_uri == "/face/suggest/") {
            return handle_suggest(event, conn, request_info);
        }
        else if (request_uri == "/face/import/") {
            return handle_import(event, conn, request_info);
        }
        else if (request_uri == "/face/stats/") {
            return handle_stats(event, conn, request_info);
        }
        else {
            return handle_invalid_request(event, conn, request_info);
        }
    }
    else {
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
