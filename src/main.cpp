
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
#include <include/types.hpp>

// C++-headers
#include <string>
#include <fstream>
#include <algorithm>

#if defined NDEBUG
#define DCERR(X)
#else
#define DCERR(X) cerr<<X;
#endif

#if !defined NMAX
#define NMAX 32
#endif

// Undefine the macro below to use C-style I/O routines.
// #define USE_CXX_IO


PhraseMap pm;
SegmentTree st;
bool building = false;
unsigned long long nreq = 0;


#define ILP_BEFORE_NON_WS  0
#define ILP_NUMERIC        1
#define ILP_BEFORE_TAB     2
#define ILP_AFTER_TAB      3
#define ILP_CHAR_DATA      4


struct InputLineParser {
    int state;
    const char *buff;
    int *pn;
    std::string *pphrase;

    InputLineParser(const char *_buff, int *_pn, std::string *_pphrase)
        : state(ILP_BEFORE_NON_WS), buff(_buff), pn(_pn), pphrase(_pphrase) {
    }

    void
    start_parsing() {
        int i = 0;
        int n = 0;
        const char *cd_start = NULL;
        int cd_len = 0;

        while (buff[i]) {
            char ch = buff[i];
            // DCERR("State: "<<this->state<<", read: "<<ch<<"\n");

            switch (this->state) {
            case ILP_BEFORE_NON_WS:
                if (!isspace(ch)) {
                    this->state = ILP_NUMERIC;
                }
                else {
                    ++i;
                }
                break;

            case ILP_NUMERIC:
                if (isdigit(ch)) {
                    n *= 10;
                    n += (ch - '0');
                    ++i;
                }
                else {
                    this->state = ILP_BEFORE_TAB;
                    on_numeric(n);
                }
                break;

            case ILP_BEFORE_TAB:
                if (ch == '\t') {
                    this->state = ILP_AFTER_TAB;
                }
                ++i;
                break;

            case ILP_AFTER_TAB:
                if (isspace(ch)) {
                    ++i;
                }
                else {
                    cd_start = this->buff + i;
                    this->state = ILP_CHAR_DATA;
                }
                break;

            case ILP_CHAR_DATA:
                // DCERR("State: ILP_CHAR_DATA: "<<buff[i]<<endl);
                ++cd_len;
                ++i;
                break;
            };
        }
        on_char_data(cd_start, cd_len);
    }

    void
    on_numeric(int n) {
        *(this->pn) = n;
    }

    void
    on_char_data(const char *data, int len) {
        if (data && *data && len) {
            // DCERR("on_char_data("<<data<<", "<<len<<")\n");
            this->pphrase->assign(data, len);
        }
    }

};


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
to_json_string(vp_t const& suggestions) {
    std::string ret = "[";
    ret.reserve(512);
    for (vp_t::const_iterator i = suggestions.begin(); i != suggestions.end(); ++i) {
        std::string p;
        p.reserve(i->phrase.size() + 10);
        for (size_t j = 0; j < i->phrase.size(); ++j) {
            if (i->phrase[j] == '"') {
                p += "\\\"";
            }
            else {
                p += i->phrase[j];
            }
        }

        std::string trailer = i + 1 == suggestions.end() ? "\n" : ",\n";
        ret += " { \"phrase\": \"" + p + "\", \"score\": " + uint_to_string(i->weight) + "}" + trailer;
    }
    ret += "]";
    return ret;
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
#if defined USE_CXX_IO
    std::ifstream fin(file.c_str());
#else
    FILE *fin = fopen(file.c_str(), "r");
#endif

    DCERR("handle_import::file:"<<file<<endl);

    if (!fin) {
        print_HTTP_response(conn, 404, "Not Found");
    }
    else {
        building = true;
        const time_t start_time = time(NULL);
        int nlines = 0;

        pm.repr.clear();
        char buff[4096];

        while (
#if defined USE_CXX_IO
               fin
#else
            !feof(fin)
#endif
               ) {

#if defined USE_CXX_IO
            fin.getline(buff, 4096);
#else
            char *got = fgets(buff, 4096, fin);
#endif

            ++nlines;

#if defined USE_CXX_IO
            const int llen = fin.gcount();
            buff[4095] = '\0';
#else
            if (!got) {
                break;
            }
            const int llen = strlen(buff);
            if (buff[llen-1] == '\n') {
                buff[llen-1] = '\0';
            }
#endif


#if 1
            int weight = 0;
            std::string phrase;
            InputLineParser(buff, &weight, &phrase).start_parsing();

            if (!phrase.empty()) {
                std::transform(phrase.begin(), phrase.end(), 
                               phrase.begin(), to_lowercase);
                // DCERR("Adding: "<<phrase<<", "<<weight<<endl);
                pm.insert(phrase, weight);
            }

#else
            int tabpos = std::find(buff, buff + llen, '\t') - buff;
            if (tabpos < llen && tabpos > 0 && tabpos < llen - 1) {
                int data_start = tabpos + 1;
                int phrase_len = llen - tabpos - 2;

                while (data_start < llen && isspace(buff[data_start])) {
                    ++data_start;
                    --phrase_len;
                }

                if (phrase_len > 0) {
                    std::string phrase(buff + data_start, phrase_len);

                    // Convert to lowercase
                    std::transform(phrase.begin(), phrase.end(), 
                                   phrase.begin(), to_lowercase);

                    buff[tabpos] = '\0';

                    uint_t weight = atoi(buff);
                    // DCERR("Adding: "<<phrase<<", "<<weight<<endl);
                    pm.insert(phrase, weight);
                }
            }
#endif
        }

        fclose(fin);
        pm.finalize();
        vui_t weights;
        for (size_t i = 0; i < pm.repr.size(); ++i) {
            weights.push_back(pm.repr[i].weight);
        }
        st.initialize(weights);

        print_HTTP_response(conn, 200, "OK");
        mg_printf(conn, "Successfully added %d/%d records from \"%s\" in %d second(s)\n", 
                  weights.size(), nlines-1, file.c_str(), time(NULL) - start_time);

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

    unsigned int n = sn.empty() ? NMAX : atoi(sn.c_str());
    if (n > NMAX) {
        n = NMAX;
    }

    vp_t results = suggest(pm, st, q, n);

    /*
      for (size_t i = 0; i < results.size(); ++i) {
      mg_printf(conn, "%s:%d\n", results[i].first.c_str(), results[i].second);
      }
    */
    mg_printf(conn, "%s\n", to_json_string(results).c_str());

    return (void*)"";
}

static void*
handle_stats(enum mg_event event,
             struct mg_connection *conn,
             const struct mg_request_info *request_info) {
    print_HTTP_response(conn, 200, "OK");
    mg_printf(conn, "Answered %d queries\n", nreq);
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
    mg_printf(conn, "Sorry, but the page you requested could not be found\n");

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
