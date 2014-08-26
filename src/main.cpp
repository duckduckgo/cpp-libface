
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
#include <include/httpserver.hpp>
#include <include/segtree.hpp>
#include <include/sparsetable.hpp>
#include <include/benderrmq.hpp>
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




PhraseMap pm;                   // Phrase Map (usually a sorted array of strings)
RMQ st;                         // An instance of the RMQ Data Structure
char *if_mmap_addr = NULL;      // Pointer to the mmapped area of the file
off_t if_length = 0;            // The length of the input file
volatile bool building = false; // TRUE if the structure is being built
unsigned long nreq = 0;         // The total number of requests served till now
int line_limit = -1;            // The number of lines to import from the input file
time_t started_at;              // When was the server started
bool opt_show_help = false;     // Was --help requested?
const char *ac_file = NULL;     // Path to the input file
int port = 6767;                // The port number on which to start the HTTP server
const char *project_homepage_url = "https://github.com/duckduckgo/cpp-libface/";

enum {
    // We are in a non-WS state
    ILP_BEFORE_NON_WS  = 0,

    // We are parsing the weight (integer)
    ILP_WEIGHT         = 1,

    // We are in the state after the weight but before the TAB
    // character separating the weight & the phrase
    ILP_BEFORE_PTAB    = 2,

    // We are in the state after the TAB character and potentially
    // before the phrase starts (or at the phrase)
    ILP_AFTER_PTAB     = 3,

    // The state parsing the phrase
    ILP_PHRASE         = 4,

    // The state after the TAB character following the phrase
    // (currently unused)
    ILP_AFTER_STAB     = 5,

    // The state in which we are parsing the snippet
    ILP_SNIPPET        = 6
};

enum { IMPORT_FILE_NOT_FOUND = 1,
       IMPORT_MUNMAP_FAILED  = 2,
       IMPORT_MMAP_FAILED    = 3
};


struct InputLineParser {
    int state;            // Current parsing state
    const char *mem_base; // Base address of the mmapped file
    const char *buff;     // A pointer to the current line to be parsed
    size_t buff_offset;   // Offset of 'buff' [above] relative to the beginning of the file. Used to index into mem_base
    int *pn;              // A pointer to any integral field being parsed
    std::string *pphrase; // A pointer to a string field being parsed

    // The input file is mmap()ped in the process' address space.

    StringProxy *psnippet_proxy; // The psnippet_proxy is a pointer to a Proxy String object that points to memory in the mmapped region

    InputLineParser(const char *_mem_base, size_t _bo, 
                    const char *_buff, int *_pn, 
                    std::string *_pphrase, StringProxy *_psp)
        : state(ILP_BEFORE_NON_WS), mem_base(_mem_base), buff(_buff), 
          buff_offset(_bo), pn(_pn), pphrase(_pphrase), psnippet_proxy(_psp)
    { }

    void
    start_parsing() {
        int i = 0;                  // The current record byte-offset.
        int n = 0;                  // Temporary buffer for numeric (integer) fields.
        const char *p_start = NULL; // Beginning of the phrase.
        const char *s_start = NULL; // Beginning of the snippet.
        int p_len = 0;              // Phrase Length.
        int s_len = 0;              // Snippet length.

        while (this->buff[i]) {
            char ch = this->buff[i];
            DCERR("["<<this->state<<":"<<ch<<"]");

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
                    this->state = ILP_SNIPPET;
                    s_start = this->buff + i + 1;
                }
                ++i;
                break;

            case ILP_SNIPPET:
                ++i;
                ++s_len;
                break;

            };
        }
        DCERR("\n");
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
            if (base < if_mmap_addr || base + len > if_mmap_addr + if_length) {
                fprintf(stderr, "base: %p, if_mmap_addr: %p, if_mmap_addr+if_length: %p\n", base, if_mmap_addr, if_mmap_addr + if_length);
                assert(base >= if_mmap_addr);
                assert(base <= if_mmap_addr + if_length);
                assert(base + len <= if_mmap_addr + if_length);
            }
            DCERR("on_snippet::base: "<<(void*)base<<", len: "<<len<<"\n");
            this->psnippet_proxy->assign(base, len);
        }
    }

};


off_t
file_size(const char *path) {
    struct stat sbuf;
    int r = stat(path, &sbuf);

    assert(r == 0);
    if (r < 0) {
        return 0;
    }

    return sbuf.st_size;
}

// TODO: Fix for > 2GiB memory usage by using uint64_t
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

char
to_lowercase(char c) {
    return std::tolower(c);
}

inline void
str_lowercase(std::string &str) {
    std::transform(str.begin(), str.end(), 
                   str.begin(), to_lowercase);

}

#define BOUNDED_RETURN(CH,LB,UB,OFFSET) if (ch >= LB && CH <= UB) { return CH - LB + OFFSET; }

inline int
hex2dec(unsigned char ch) {
    BOUNDED_RETURN(ch, '0', '9', 0);
    BOUNDED_RETURN(ch, 'A', 'F', 10);
    BOUNDED_RETURN(ch, 'a', 'f', 10);
    return 0;
}

#undef BOUNDED_RETURN

std::string
unescape_query(std::string const &query) {
    enum {
        QP_DEFAULT = 0,
        QP_ESCAPED1 = 1,
        QP_ESCAPED2 = 2
    };
    std::string ret;
    unsigned char echar = 0;
    int state = QP_DEFAULT;
    for (size_t i = 0; i < query.size(); ++i) {
        switch (state) {
        case QP_DEFAULT:
            if (query[i] != '%') {
                ret += query[i];
            } else {
                state = QP_ESCAPED1;
                echar = 0;
            }
            break;
        case QP_ESCAPED1:
        case QP_ESCAPED2:
            echar *= 16;
            echar += hex2dec(query[i]);
            if (state == QP_ESCAPED2) {
                ret += echar;
            }
            state = (state + 1) % 3;
        }
    }
    return ret;
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
            (snippet.empty() ? "" : ", \"snippet\": \"" + snippet + "\"") + " }" + trailer;
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

bool is_EOF(FILE *pf) { return feof(pf); }
bool is_EOF(std::ifstream fin) { return !!fin; }

void get_line(FILE *pf, char *buff, int buff_len, int &read_len) {
    char *got = fgets(buff, INPUT_LINE_SIZE, pf);
    if (!got) {
        read_len = -1;
        return;
    }
    read_len = strlen(buff);
    if (read_len > 0 && buff[read_len - 1] == '\n') {
        buff[read_len - 1] = '\0';
    }
}

void get_line(std::ifstream fin, char *buff, int buff_len, int &read_len) {
    fin.getline(buff, buff_len);
    read_len = fin.gcount();
    buff[INPUT_LINE_SIZE - 1] = '\0';
}


int
do_import(std::string file, uint_t limit, 
          int &rnadded, int &rnlines) {
    bool is_input_sorted = true;
#if defined USE_CXX_IO
    std::ifstream fin(file.c_str());
#else
    FILE *fin = fopen(file.c_str(), "r");
#endif

    int fd = open(file.c_str(), O_RDONLY);

    DCERR("handle_import::file:" << file << "[fin: " << (!!fin) << ", fd: " << fd << "]" << endl);

    if (!fin || fd == -1) {
        perror("fopen");
        return -IMPORT_FILE_NOT_FOUND;
    }
    else {
        building = true;
        int nlines = 0;
        int foffset = 0;

        if (if_mmap_addr) {
            int r = munmap(if_mmap_addr, if_length);
            if (r < 0) {
                perror("munmap");
                building = false;
                return -IMPORT_MUNMAP_FAILED;
            }
        }

        // Potential race condition + not checking for return value
        if_length = file_size(file.c_str());

        // mmap() the input file in
        if_mmap_addr = (char*)mmap(NULL, if_length, PROT_READ, MAP_SHARED, fd, 0);
        if (if_mmap_addr == MAP_FAILED) {
            fprintf(stderr, "length: %llu, fd: %d\n", if_length, fd);
            perror("mmap");
            if (fin) { fclose(fin); }
            if (fd != -1) { close(fd); }
            building = false;
            return -IMPORT_MMAP_FAILED;
        }

        pm.repr.clear();
        char buff[INPUT_LINE_SIZE];
        std::string prev_phrase;

        while (!is_EOF(fin) && limit--) {
            buff[0] = '\0';

            int llen = -1;
            get_line(fin, buff, INPUT_LINE_SIZE, llen);
            if (llen == -1) {
                break;
            }

            ++nlines;

            int weight = 0;
            std::string phrase;
            StringProxy snippet;
            InputLineParser(if_mmap_addr, foffset, buff, &weight, &phrase, &snippet).start_parsing();

            foffset += llen;

            if (!phrase.empty()) {
                str_lowercase(phrase);
                DCERR("Adding: " << weight << ", " << phrase << ", " << std::string(snippet) << endl);
                pm.insert(weight, phrase, snippet);
            }
            if (is_input_sorted && prev_phrase <= phrase) {
                prev_phrase.swap(phrase);
            } else if (is_input_sorted) {
                is_input_sorted = false;
            }
        }

        DCERR("Creating PhraseMap::Input is " << (!is_input_sorted ? "NOT " : "") << "sorted\n");

        fclose(fin);
        pm.finalize(is_input_sorted);
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

static void handle_import(client_t *client, parsed_url_t &url) {
    std::string body;
    headers_t headers;
    headers["Cache-Control"] = "no-cache";

    std::string file = unescape_query(url.query["file"]);
    uint_t limit     = atoi(url.query["limit"].c_str());
    int nadded, nlines;
    const time_t start_time = time(NULL);

    if (!limit) {
        limit = minus_one;
    }

    int ret = do_import(file, limit, nadded, nlines);
    if (ret < 0) {
        switch (-ret) {
        case IMPORT_FILE_NOT_FOUND:
            body = "The file '" + file + "' was not found";
            write_response(client, 404, "Not Found", headers, body);
            break;

        case IMPORT_MUNMAP_FAILED:
            body = "munmap(2) failed";
            write_response(client, 500, "Internal Server Error", headers, body);
            break;

        case IMPORT_MMAP_FAILED:
            body = "mmap(2) failed";
            write_response(client, 500, "Internal Server Error", headers, body);
            break;

        default:
            body = "Unknown Error";
            write_response(client, 500, "Internal Server Error", headers, body);
            cerr<<"ERROR::Unknown error: "<<ret<<endl;
        }
    }
    else {
        std::ostringstream os;
        os << "Successfully added " << nadded << "/" << nlines
           << "records from '" << file << "' in " << (time(NULL) - start_time)
           << "second(s)\n";
        body = os.str();
        write_response(client, 200, "OK", headers, body);
    }
}

static void handle_export(client_t *client, parsed_url_t &url) {
    std::string body;
    headers_t headers;
    headers["Cache-Control"] = "no-cache";

    std::string file = url.query["file"];
    if (building) {
        body = "Busy\n";
        write_response(client, 412, "Busy", headers, body);
        return;
    }

    // Prevent modifications to 'pm' while we export
    building = true;
    ofstream fout(file.c_str());
    const time_t start_time = time(NULL);

    for (size_t i = 0; i < pm.repr.size(); ++i) {
        fout<<pm.repr[i].weight<<'\t'<<pm.repr[i].phrase<<'\t'<<std::string(pm.repr[i].snippet)<<'\n';
    }

    building = false;
    std::ostringstream os;
    os << "Successfully wrote " << pm.repr.size()
       << " records to output file '" << file
       << "' in " << (time(NULL) - start_time) << "second(s)\n";
    body = os.str();
    write_response(client, 200, "OK", headers, body);
}

static void handle_suggest(client_t *client, parsed_url_t &url) {
    ++nreq;
    std::string body;
    headers_t headers;
    headers["Cache-Control"] = "no-cache";

    if (building) {
        write_response(client, 412, "Busy", headers, body);
        return;
    }

    std::string q    = unescape_query(url.query["q"]);
    std::string sn   = url.query["n"];
    std::string cb   = unescape_query(url.query["callback"]);
    std::string type = unescape_query(url.query["type"]);

    DCERR("handle_suggest::q:"<<q<<", sn:"<<sn<<", callback: "<<cb<<endl);

    unsigned int n = sn.empty() ? NMAX : atoi(sn.c_str());
    if (n > NMAX) {
        n = NMAX;
    }
    if (n < 1) {
        n = 1;
    }

    const bool has_cb = !cb.empty();
    str_lowercase(q);
    vp_t results = suggest(pm, st, q, n);

    /*
      for (size_t i = 0; i < results.size(); ++i) {
      mg_printf(conn, "%s:%d\n", results[i].first.c_str(), results[i].second);
      }
    */
    headers["Content-Type"] = "application/json; charset=UTF-8";
    if (has_cb) {
        body = cb + "(" + results_json(q, results, type) + ");\n";
    }
    else {
        body = results_json(q, results, type) + "\n";
    }

    write_response(client, 200, "OK", headers, body);
}

static void handle_stats(client_t *client, parsed_url_t &url) {
    headers_t headers;
    headers["Cache-Control"] = "no-cache";

    std::string body;
    char buff[2048];
    char *b = buff;
    b += sprintf(b, "Answered %lu queries\n", nreq);
    b += sprintf(b, "Uptime: %s\n", get_uptime().c_str());

    if (building) {
        b += sprintf(b, "Data Store is busy\n");
    }
    else {
        b += sprintf(b, "Data store size: %d entries\n", pm.repr.size());
    }
    b += sprintf(b, "Memory usage: %d MiB\n", get_memory_usage(getpid())/1024);
    body = buff;
    write_response(client, 200, "OK", headers, body);
}

static void handle_invalid_request(client_t *client, parsed_url_t &url) {
    headers_t headers;
    headers["Cache-Control"] = "no-cache";

    std::string body = "Sorry, but the page you requested could not be found\n";
    write_response(client, 404, "Not Found", headers, body);
}


void serve_request(client_t *client) {
    parsed_url_t url;
    parse_URL(client->url, url);
    std::string &request_uri = url.path;
    DCERR("request_uri: " << request_uri << endl);

    if (request_uri == "/face/suggest/") {
        handle_suggest(client, url);
    }
    else if (request_uri == "/face/import/") {
        handle_import(client, url);
    }
    else if (request_uri == "/face/export/") {
        handle_export(client, url);
    }
    else if (request_uri == "/face/stats/") {
        handle_stats(client, url);
    }
    else {
        handle_invalid_request(client, url);
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
    printf("-l, --limit=LIMIT    Load only the first LIMIT lines from PATH (default: -1 [unlimited])\n");
    printf("\n");
    printf("Please visit %s for more information.\n", project_homepage_url);
}

void
parse_options(int argc, char *argv[]) {
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"file", 1, 0, 'f'},
            {"port", 1, 0, 'p'},
            {"limit", 1, 0, 'l'},
            {"help", 0, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "f:p:l:h",
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
            port = atoi(optarg);
            break;

        case 'h':
            opt_show_help = true;
            break;

        case 'l':
            line_limit = atoi(optarg);
            DCERR("Limit # of lines to: " << line_limit << endl);
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

    started_at = time(NULL);

    cerr<<"INFO::Starting lib-face on port '"<<port<<"'\n";

    if (ac_file) {
        int nadded, nlines;
        const time_t start_time = time(NULL);
        int ret = do_import(ac_file, line_limit, nadded, nlines);
        if (ret < 0) {
            switch (-ret) {
            case IMPORT_FILE_NOT_FOUND:
                fprintf(stderr, "The file '%s' was not found\n", ac_file);
                break;

            case IMPORT_MUNMAP_FAILED:
                fprintf(stderr, "munmap(2) on file '%s' failed\n", ac_file);
                break;

            case IMPORT_MMAP_FAILED:
                fprintf(stderr, "mmap(2) on file '%s' failed\n", ac_file);
                break;

            default:
                cerr<<"ERROR::Unknown error: "<<ret<<endl;
            }
            return 1;
        }
        else {
            fprintf(stderr, "INFO::Successfully added %d/%d records from \"%s\" in %d second(s)\n", 
                    nadded, nlines, ac_file, (int)(time(NULL) - start_time));
        }
    }

    int r = httpserver_start(&serve_request, "0.0.0.0", port);
    if (r != 0) {
        fprintf(stderr, "ERROR::Could not start the web server\n");
        return 1;
    }
    return 0;
}
