
// C-headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Custom-includes
#include "mongoose.h"
#include <include/segtree.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>

// C++-headers
#include <string>
#include <fstream>
#include <algorithm>

#if defined NDEBUG
#define DCERR(X)
#else
#define DCERR(X) cerr<<X;
#endif


PhraseMap pm;
SegmentTree st;
bool building = false;
unsigned long long nreq = 0;


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

char
to_lowercase(char c) {
    return std::tolower(c);
}


std::string
uint_to_string(uint_t n) {
    std::string ret;
    if (!n) {
        ret = "0";
    }
    else {
        while (n) {
            ret.insert(0, 1, ('0' + (n % 10)));
            n /= 10;
        }
    }
    return ret;
}

std::string
to_json_string(vpsui_t const& suggestions) {
    std::string ret = "[";
    ret.reserve(512);
    for (vpsui_t::const_iterator i = suggestions.begin(); i != suggestions.end(); ++i) {
        std::string p;
        p.reserve(i->first.size() + 10);
        for (size_t j = 0; j < i->first.size(); ++j) {
            if (i->first[j] == '"') {
                p += "\\\"";
            }
            else {
                p += i->first[j];
            }
        }

        std::string trailer = i + 1 == suggestions.end() ? "\n" : ",\n";
        ret += " { \"phrase\": \"" + p + "\", \"score\": " + uint_to_string(i->second) + "}" + trailer;
    }
    ret += "]";
    return ret;
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
    std::ifstream fin(file.c_str());

    DCERR("handle_import::file:"<<file<<endl);

    if (!fin) {
        print_HTTP_response(conn, 404, "Not Found");
    }
    else {
        building = true;
        time_t start_time = time(NULL);

        pm.repr.clear();
        char buff[4096];

        while (fin) {
            fin.getline(buff, 4096);
            int llen = fin.gcount();
            buff[4095] = '\0';
            int tabpos = std::find(buff, buff + llen, '\t') - buff;
            if (tabpos < llen && tabpos > 0 && tabpos < llen - 1) {
                std::string phrase(buff + tabpos + 1, llen - tabpos - 2);
                size_t data_start = 0;
                while (data_start < phrase.size() && isspace(phrase[data_start])) {
                    ++data_start;
                }
                if (data_start > 0) {
                    phrase = phrase.substr(data_start);
                }

                // Convert to lowercase
                std::transform(phrase.begin(), phrase.end(), 
                               phrase.begin(), to_lowercase);

                buff[tabpos] = '\0';

                uint_t weight = atoi(buff);
                DCERR("Adding: "<<phrase<<", "<<weight<<endl);
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
        mg_printf(conn, "Successfully added %d records from \"%s\" in %d second(s).", 
                  weights.size(), file.c_str(), time(NULL) - start_time);

        building = false;
    }

    return (void*)"";
}

static void*
handle_suggest(enum mg_event event,
               struct mg_connection *conn,
               const struct mg_request_info *request_info) {

    ++nreq;
    if (building) {
        print_HTTP_response(conn, 412, "Busy");
        return (void*)"";
    }

    print_HTTP_response(conn, 200, "OK");

    std::string q  = get_qs(request_info, "q");
    std::string sn = get_qs(request_info, "n");

    DCERR("handle_suggest::q:"<<q<<", sn:"<<sn<<endl);

    unsigned int n = sn.empty() ? 16 : atoi(sn.c_str());
    if (n > 16) {
        n = 16;
    }

    vpsui_t results = suggest(pm, st, q, n);

    /*
      for (size_t i = 0; i < results.size(); ++i) {
      mg_printf(conn, "%s:%d\n", results[i].first.c_str(), results[i].second);
      }
    */
    mg_printf(conn, "%s", to_json_string(results).c_str());

    return (void*)"";
}

static void*
handle_stats(enum mg_event event,
             struct mg_connection *conn,
             const struct mg_request_info *request_info) {
    print_HTTP_response(conn, 200, "OK");
    mg_printf(conn, "Handled: %d connections\n", nreq);
    if (building) {
        mg_printf(conn, "Currently building data store\n");
    }
    else {
        mg_printf(conn, "Data store size: %d entries\n", pm.repr.size());
    }

    return (void*)"";
}

static void*
handle_invalid_request(enum mg_event event,
                       struct mg_connection *conn,
                       const struct mg_request_info *request_info) {
    print_HTTP_response(conn, 404, "Not Found");
    mg_printf(conn, "Sorry, but the page you requested could not be found");

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
