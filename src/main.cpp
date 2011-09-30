
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


// Max. line size is 8191 bytes.
#define INPUT_LINE_SIZE 8192

// How many bytes to reserve for the output string
#define OUTPUT_SIZE_RESERVE 4096


// Undefine the macro below to use C-style I/O routines.
// #define USE_CXX_IO


PhraseMap pm;
SegmentTree st;
bool building = false;
unsigned long long nreq = 0;
time_t started_at;


#define ILP_BEFORE_NON_WS  0
#define ILP_WEIGHT         1
#define ILP_BEFORE_PTAB    2
#define ILP_AFTER_PTAB     3
#define ILP_PHRASE         4
#define ILP_AFTER_STAB     5
#define ILP_SNIPPET        6



struct InputLineParser {
    int state;
    const char *buff;
    int *pn;
    std::string *pphrase, *psnippet;

    InputLineParser(const char *_buff, int *_pn, 
                    std::string *_pphrase, std::string *_psnippet)
        : state(ILP_BEFORE_NON_WS), buff(_buff), pn(_pn), 
          pphrase(_pphrase), psnippet(_psnippet) {
    }

    void
    start_parsing() {
        int i = 0;
        int n = 0;
        const char *p_start = NULL, *s_start = NULL;
        int p_len = 0, s_len = 0;

        while (buff[i]) {
            char ch = buff[i];
            DCERR("State: "<<this->state<<", read: "<<ch<<"\n");

            switch (this->state) {
            case ILP_BEFORE_NON_WS:
                if (!isspace(ch)) {
                    this->state = ILP_WEIGHT;
                }
                else {
                    ++i;
                }
                break;

            case ILP_WEIGHT:
                if (isdigit(ch)) {
                    n *= 10;
                    n += (ch - '0');
                    ++i;
                }
                else {
                    this->state = ILP_BEFORE_PTAB;
                    on_weight(n);
                }
                break;

            case ILP_BEFORE_PTAB:
                if (ch == '\t') {
                    this->state = ILP_AFTER_PTAB;
                }
                ++i;
                break;

            case ILP_AFTER_PTAB:
                if (isspace(ch)) {
                    ++i;
                }
                else {
                    p_start = this->buff + i;
                    this->state = ILP_PHRASE;
                }
                break;

            case ILP_PHRASE:
                // DCERR("State: ILP_PHRASE: "<<buff[i]<<endl);
                if (ch != '\t') {
                    ++p_len;
                }
                else {
                    this->state = ILP_AFTER_STAB;
                }
                ++i;
                break;

            case ILP_AFTER_STAB:
                if (!isspace(ch)) {
                    this->state = ILP_SNIPPET;
                    s_start = this->buff + i;
                }
                else {
                    ++i;
                }
                break;

            case ILP_SNIPPET:
                ++i;
                ++s_len;
                break;

            };
        }
        on_phrase(p_start, p_len);
        on_snippet(s_start, s_len);
    }

    void
    on_weight(int n) {
        *(this->pn) = n;
    }

    void
    on_phrase(const char *data, int len) {
        if (len && this->pphrase) {
            // DCERR("on_phrase("<<data<<", "<<len<<")\n");
            this->pphrase->assign(data, len);
        }
    }

    void
    on_snippet(const char *data, int len) {
        if (len && this->psnippet) {
            this->psnippet->assign(data, len);
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
              "Cache-Control: no-cache\r\n"
              "Content-Type: %s\r\n\r\n", code, description, content_type);
}

char
to_lowercase(char c) {
    return std::tolower(c);
}

inline void
str_lowercase(std::string &str) {
    std::transform(str.begin(), str.end(), 
                   str.begin(), to_lowercase);

}


inline std::string
uint_to_string(uint_t n, uint_t pad = 0) {
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
    while (pad && ret.size() < pad) {
        ret = "0" + ret;
    }

    return ret;
}

void
escape_quotes(std::string& str) {
    std::string ret;
    ret.reserve(str.size() + 10);
    for (size_t j = 0; j < str.size(); ++j) {
        if (str[j] == '"') {
            ret += "\\\"";
        }
        else {
            ret += str[j];
        }
    }
    ret.swap(str);
}

std::string
to_json_string(vp_t& suggestions) {
    std::string ret = "[";
    ret.reserve(OUTPUT_SIZE_RESERVE);
    for (vp_t::iterator i = suggestions.begin(); i != suggestions.end(); ++i) {
        escape_quotes(i->phrase);
        escape_quotes(i->snippet);

        std::string trailer = i + 1 == suggestions.end() ? "\n" : ",\n";
        ret += " { \"phrase\": \"" + i->phrase + "\", \"score\": " + uint_to_string(i->weight) + 
            (i->snippet.empty() ? "" : ", \"snippet\": \"" + i->snippet + "\"") + "}" + trailer;
    }
    ret += "]";
    return ret;
}

std::string
pluralize(std::string s, int n) {
    return n>1 ? s+"s" : s;
}

std::string
humanized_time_difference(time_t prev, time_t curr) {
    std::string ret = "";
    if (prev > curr) {
        std::swap(prev, curr);
    }

    if (prev == curr) {
        return "just now";
    }

    int sec = curr - prev;
    ret = uint_to_string(sec % 60, 2) + ret;

    int minute = sec / 60;
    ret = uint_to_string(minute % 60, 2) + ":" + ret;

    int hour = minute / 60;
    ret = uint_to_string(hour % 24, 2) + ":" + ret;

    int day = hour / 24;
    if (day) {
        ret = uint_to_string(day % 7) + pluralize(" day", day%7) + " " + ret;
    }

    int week = day / 7;
    if (week) {
        ret = uint_to_string(week % 4) + pluralize(" day", week%4) + " " + ret;
    }

    int month = week / 4;
    if (month) {
        ret = uint_to_string(month) + pluralize(" month", month) + " " + ret;
    }

    return ret;
}


std::string
get_uptime() {
    return humanized_time_difference(started_at, time(NULL));
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
    int sorted = atoi(get_qs(request_info, "sorted").c_str());

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
        char buff[INPUT_LINE_SIZE];

        while (
#if defined USE_CXX_IO
               fin
#else
            !feof(fin)
#endif
               ) {

#if defined USE_CXX_IO
            fin.getline(buff, INPUT_LINE_SIZE);
#else
            char *got = fgets(buff, INPUT_LINE_SIZE, fin);
#endif

            ++nlines;

#if defined USE_CXX_IO
            const int llen = fin.gcount();
            buff[INPUT_LINE_SIZE - 1] = '\0';
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
            std::string phrase, snippet;
            InputLineParser(buff, &weight, &phrase, &snippet).start_parsing();

            if (!phrase.empty()) {
                str_lowercase(phrase);
                DCERR("Adding: "<<weight<<", "<<phrase<<", "<<snippet<<endl);
                pm.insert(weight, phrase, snippet);
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
        pm.finalize(sorted);
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
handle_export(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
    if (building) {
        print_HTTP_response(conn, 412, "Busy");
        mg_printf(conn, "Busy\n");
        return (void*)"";
    }

    // Prevent modifications to 'pm' while we export
    building = true;
    ofstream fout(file.c_str());
    const time_t start_time = time(NULL);

    for (size_t i = 0; i < pm.repr.size(); ++i) {
        fout<<pm.repr[i].weight<<'\t'<<pm.repr[i].phrase<<'\n';
    }

    building = false;
    mg_printf(conn, "Successfully wrote %u records to output file '%s' in %d second(s)\n", 
              pm.repr.size(), file.c_str(), time(NULL) - start_time);
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

    str_lowercase(q);
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
    mg_printf(conn, "Uptime: %s\n", get_uptime().c_str());

    if (building) {
        mg_printf(conn, "Data Store is busy\n");
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
        else if (request_uri == "/face/export/") {
            return handle_export(event, conn, request_info);
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

  started_at = time(NULL);

  ctx = mg_start(&callback, NULL, options);
  while (1) {
    // Never stop
    sleep(100);
  }
  mg_stop(ctx);

  return 0;
}
