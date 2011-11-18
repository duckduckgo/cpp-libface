
// C-headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include <fcntl.h>


// Custom-includes
#include "mongoose.h"
#include <include/segtree.hpp>
#include <include/sparsetable.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>
#include <include/types.hpp>
#include <include/utils.hpp>

// C++-headers
#include <string>
#include <fstream>
#include <algorithm>

#if !defined NMAX
#define NMAX 32
#endif


#if !defined INPUT_LINE_SIZE
// Max. line size is 8191 bytes.
#define INPUT_LINE_SIZE 8192
#endif

// How many bytes to reserve for the output string
#define OUTPUT_SIZE_RESERVE 4096

// Undefine the macro below to use C-style I/O routines.
// #define USE_CXX_IO




PhraseMap pm;
RMQ st;
char *if_mmap_addr = NULL;
size_t if_length = 0;
bool building = false;
unsigned long long nreq = 0;
time_t started_at;
bool ac_sorted = false;
bool opt_show_help = false;
const char *ac_file = NULL;
const char *port = "6767";

// We are in a non-WS state
#define ILP_BEFORE_NON_WS  0
// We are parsing the weight (integer)
#define ILP_WEIGHT         1
// We are in the state after the weight but before the TAB character
// separating the weight & the phrase
#define ILP_BEFORE_PTAB    2
// We are in the state after the TAB character and potentially before
// the phrase starts (or at the phrase)
#define ILP_AFTER_PTAB     3
// The state parsing the phrase
#define ILP_PHRASE         4
// The state after the TAB character following the phrase (currently
// unused)
#define ILP_AFTER_STAB     5
// The state in which we are parsing the snippet
#define ILP_SNIPPET        6


#define IMPORT_FILE_NOT_FOUND 1



struct InputLineParser {
    int state;
    const char *mem_base, *buff;
    size_t buff_offset;
    int *pn;
    std::string *pphrase;
    // The input file is mmap()ped in the process' address space.
    StringProxy *psnippet_proxy;

    InputLineParser(const char *_mem_base, size_t _bo, 
                    const char *_buff, int *_pn, 
                    std::string *_pphrase, StringProxy *_psp)
        : state(ILP_BEFORE_NON_WS), mem_base(_mem_base), buff(_buff), 
          buff_offset(_bo), pn(_pn), pphrase(_pphrase), psnippet_proxy(_psp)
    { }

    void
    start_parsing() {
        int i = 0;
        int n = 0;
        const char *p_start = NULL, *s_start = NULL;
        int p_len = 0, s_len = 0;

        while (this->buff[i]) {
            char ch = this->buff[i];
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
                    // Note: Skip to ILP_SNIPPET since the snippet may
                    // start with a white-space that we wish to
                    // preserve.
                    // 
                    // this->state = ILP_AFTER_STAB;
                    this->state = ILP_SNIPPET;
                    s_start = this->buff + i + 1;
                }
                ++i;
                break;

            case ILP_AFTER_STAB:
                if (isspace(ch)) {
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
        if (len && this->psnippet_proxy) {
            const char *base = this->mem_base + this->buff_offset + 
                (data - this->buff);
            DCERR("on_snippet::base: "<<(void*)base<<", len: "<<len<<"\n");
            this->psnippet_proxy->assign(base, len);
        }
    }

};


size_t
file_size(const char *path) {
    struct stat sbuf;
    int r = stat(path, &sbuf);

    assert(r == 0);
    if (r < 0) {
        return 0;
    }

    return sbuf.st_size;
}

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

int
get_memory_usage(pid_t pid) {
    char cbuff[4096];
    sprintf(cbuff, "pmap -x %d | tail -n +3 | awk 'BEGIN { S=0;T=0 } { if (match($3, /\\-/)) {S=1} if (S==0) {T+=$3} } END { print T }'", pid);
    FILE *pf = popen(cbuff, "r");
    if (!pf) {
        return -1;
    }
    int r = fread(cbuff, 1, 100, pf);
    if (r < 0) {
        fclose(pf);
        return -1;
    }
    cbuff[r-1] = '\0';
    r = atoi(cbuff);
    fclose(pf);
    return r;
}

void
print_HTTP_response(struct mg_connection *conn, 
                    int code, const char *description, 
                    const char *content_type = "text/plain; charset=UTF-8") {
    mg_printf(conn, "HTTP/1.0 %d %s\r\n"
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
escape_special_chars(std::string& str) {
    std::string ret;
    ret.reserve(str.size() + 10);
    for (size_t j = 0; j < str.size(); ++j) {
        switch (str[j]) {
        case '"':
            ret += "\\\"";
            break;

        case '\\':
            ret += "\\\\";
            break;

        case '\n':
            ret += "\\n";
            break;

        case '\t':
            ret += "\\t";
            break;

        default:
            ret += str[j];
        }
    }
    ret.swap(str);
}

std::string
rich_suggestions_json_array(vp_t& suggestions) {
    std::string ret = "[";
    ret.reserve(OUTPUT_SIZE_RESERVE);
    for (vp_t::iterator i = suggestions.begin(); i != suggestions.end(); ++i) {
        escape_special_chars(i->phrase);
        std::string snippet = i->snippet;
        escape_special_chars(snippet);

        std::string trailer = i + 1 == suggestions.end() ? "\n" : ",\n";
        ret += " { \"phrase\": \"" + i->phrase + "\", \"score\": " + uint_to_string(i->weight) + 
            (snippet.empty() ? "" : ", \"snippet\": \"" + snippet + "\"") + "}" + trailer;
    }
    ret += "]";
    return ret;
}

std::string
suggestions_json_array(vp_t& suggestions) {
    std::string ret = "[";
    ret.reserve(OUTPUT_SIZE_RESERVE);
    for (vp_t::iterator i = suggestions.begin(); i != suggestions.end(); ++i) {
        escape_special_chars(i->phrase);

        std::string trailer = i + 1 == suggestions.end() ? "\n" : ",\n";
        ret += "\"" + i->phrase + "\"" + trailer;
    }
    ret += "]";
    return ret;
}

std::string
results_json(std::string q, vp_t& suggestions, std::string const& type) {
    if (type == "list") {
        escape_special_chars(q);
        return "[ \"" + q + "\", " + suggestions_json_array(suggestions) + " ]";
    }
    else {
        return rich_suggestions_json_array(suggestions);
    }
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

bool
is_valid_cb(std::string const& cb) {
    // We don't check since this could be any valid javascript expression
    return true;
}

int
do_import(std::string file, int sorted, uint_t limit, 
          int &rnadded, int &rnlines) {
#if defined USE_CXX_IO
    std::ifstream fin(file.c_str());
#else
    FILE *fin = fopen(file.c_str(), "r");
#endif

    int fd = open(file.c_str(), O_RDONLY);

    // Potential race condition + not checking for return value
    if_length = file_size(file.c_str());

    DCERR("handle_import::file:"<<file<<endl);

    if (!fin || !fd) {
        return -IMPORT_FILE_NOT_FOUND;
    }
    else {
        building = true;
        int nlines = 0;
        int foffset = 0;

        if (if_mmap_addr) {
            munmap(if_mmap_addr, if_length);
        }

        // mmap() the input file in
        if_mmap_addr = (char*)mmap(NULL, if_length, PROT_READ, MAP_SHARED, fd, 0);
        if (!if_mmap_addr) {
            fclose(fin);
            close(fd);
            return -IMPORT_FILE_NOT_FOUND;
        }

        pm.repr.clear();
        char buff[INPUT_LINE_SIZE];

        while (
#if defined USE_CXX_IO
               fin
#else
            !feof(fin)
#endif
               && limit--) {

            buff[0] = '\0';

#if defined USE_CXX_IO
            fin.getline(buff, INPUT_LINE_SIZE);
            const int llen = fin.gcount();
            buff[INPUT_LINE_SIZE - 1] = '\0';
#else
            char *got = fgets(buff, INPUT_LINE_SIZE, fin);
            if (!got) {
                break;
            }
            const int llen = strlen(buff);
            if (llen && buff[llen-1] == '\n') {
                buff[llen-1] = '\0';
            }
#endif

            ++nlines;

            int weight = 0;
            std::string phrase;
            StringProxy snippet;
            InputLineParser(if_mmap_addr, foffset, buff, &weight, &phrase, &snippet).start_parsing();

            foffset += llen;

            if (!phrase.empty()) {
                str_lowercase(phrase);
                DCERR("Adding: "<<weight<<", "<<phrase<<", "<<std::string(snippet)<<endl);
                pm.insert(weight, phrase, snippet);
            }
        }

        fclose(fin);
        pm.finalize(sorted);
        vui_t weights;
        for (size_t i = 0; i < pm.repr.size(); ++i) {
            weights.push_back(pm.repr[i].weight);
        }
        st.initialize(weights);

        rnadded = weights.size();
        rnlines = nlines;

        building = false;
    }

    return 0;
}

static void*
handle_import(enum mg_event event,
              struct mg_connection *conn,
              const struct mg_request_info *request_info) {
    std::string file = get_qs(request_info, "file");
    int sorted = atoi(get_qs(request_info, "sorted").c_str());
    uint_t limit  = atoi(get_qs(request_info, "limit").c_str());
    int nadded, nlines;
    const time_t start_time = time(NULL);

    if (!limit) {
        limit = minus_one;
    }

    int ret = do_import(file, sorted, limit, nadded, nlines);
    if (ret < 0) {
        switch (-ret) {
        case IMPORT_FILE_NOT_FOUND:
            print_HTTP_response(conn, 404, "Not Found");
            break;

        default:
            cerr<<"ERROR::Unknown error: "<<ret<<endl;
        }
    }
    else {
        print_HTTP_response(conn, 200, "OK");
        mg_printf(conn, "Successfully added %d/%d records from \"%s\" in %d second(s)\n", 
                  nadded, nlines, file.c_str(), time(NULL) - start_time);
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
        fout<<pm.repr[i].weight<<'\t'<<pm.repr[i].phrase<<'\t'<<std::string(pm.repr[i].snippet)<<'\n';
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

    std::string q    = get_qs(request_info, "q");
    std::string sn   = get_qs(request_info, "n");
    std::string cb   = get_qs(request_info, "callback");
    std::string type = get_qs(request_info, "type");

    DCERR("handle_suggest::q:"<<q<<", sn:"<<sn<<", callback: "<<cb<<endl);

    unsigned int n = sn.empty() ? NMAX : atoi(sn.c_str());
    if (n > NMAX) {
        n = NMAX;
    }

    const bool has_cb = !cb.empty();
    if (has_cb && !is_valid_cb(cb)) {
        print_HTTP_response(conn, 400, "Invalid Request");
        return (void*)"";
    }

    print_HTTP_response(conn, 200, "OK");

    str_lowercase(q);
    vp_t results = suggest(pm, st, q, n);

    /*
      for (size_t i = 0; i < results.size(); ++i) {
      mg_printf(conn, "%s:%d\n", results[i].first.c_str(), results[i].second);
      }
    */
    if (has_cb) {
        mg_printf(conn, "%s(%s);\n", cb.c_str(), results_json(q, results, type).c_str());
    }
    else {
        mg_printf(conn, "%s\n", results_json(q, results, type).c_str());
    }

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
    mg_printf(conn, "Memory usage: %d MiB\n", get_memory_usage(getpid())/1024);

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


void
show_usage(char *argv[]) {
    printf("Usage: %s [OPTION]...\n", basename(argv[0]));
    printf("Start lib-face.\n\n");
    printf("Optional arguments:\n\n");
    printf("-h, --help           This screen\n");
    printf("-f, --file=PATH      Path of the file containing the phrases\n");
    printf("-p, --port=PORT      TCP port on which to start lib-face (default: 6767)\n");
    printf("-s, --sorted         If specified, the input file (PATH) is assumed to be sorted by phrase\n");
    printf("\n");
    printf("Please visit https://code.google.com/p/lib-face/ for more information.\n");
}

void
parse_options(int argc, char *argv[]) {
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"file", 1, 0, 'f'},
            {"port", 1, 0, 'p'},
            {"sorted", 0, 0, 's'},
            {"help", 0, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "f:p:sh",
                        long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 0:
        case 'f':
            DCERR("File: "<<optarg<<endl);
            ac_file = optarg;
            break;

        case 'p':
            DCERR("Port: "<<optarg<<" ("<<atoi(optarg)<<")\n");
            port = optarg;
            break;

        case 's':
            DCERR("File is Sorted\n");
            ac_sorted = true;
            break;
          
        case 'h':
            opt_show_help = true;
            break;

        case '?':
            cerr<<"ERROR::Invalid option: "<<optopt<<endl;
            break;
        }
    }


}


int
main(int argc, char* argv[]) {
    parse_options(argc, argv);
    if (opt_show_help) {
        show_usage(argv);
        return 0;
    }

    struct mg_context *ctx;
    const char *options[] = {"listening_ports", port, NULL};

    started_at = time(NULL);

    cerr<<"INFO::Starting lib-face on port '"<<port<<"'\n";

    if (ac_file) {
        int nadded, nlines;
        const time_t start_time = time(NULL);
        int ret = do_import(ac_file, ac_sorted, minus_one, nadded, nlines);
        if (ret < 0) {
            fprintf(stderr, "ERROR::Could not add lines in file '%s'\n", ac_file);
        }
        else {
            fprintf(stderr, "INFO::Successfully added %d/%d records from \"%s\" in %d second(s)\n", 
                    nadded, nlines, ac_file, (int)(time(NULL) - start_time));
        }
    }

    ctx = mg_start(&callback, NULL, options);
    if (!ctx) {
        fprintf(stderr, "ERROR::Could not start the web server\n");
        return 1;
    }

    while (1) {
        // Never stop
        sleep(100);
    }
    mg_stop(ctx);

    return 0;
}
