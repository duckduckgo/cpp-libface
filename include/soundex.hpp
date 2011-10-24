// Code adapted from: http://rosettacode.org/wiki/Soundex

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
 
/* for ASCII only */
static char code[128] = { 0 };
void add_code(const char *s, int c)
{
    while (*s) {
        code[(int)*s] = code[0x20 ^ (int)*s] = c;
        s++;
    }
}
 
void init()
{
    static const char *cls[] =
        { "AEIOUHWY", "", "BFPV", "CGJKQSXZ", "DT", "L", "MN", "R", 0};
    int i;
    for (i = 0; cls[i]; i++)
        add_code(cls[i], i - 1);
}

// out must be 'oc+1' characters long
char*
c_soundex(const char *s, char *out, int oc)
{
    int c, prev, i;

    out[0] = out[oc] = 0;
    if (!s || !*s) return out;

    out[0] = *s++;
 
    /* first letter, though not coded, can still affect next letter: Pfister */
    // prev = code[(int)out[0]];
    // cerr<<"code[(int)out[0]]: "<<(int)code[(int)out[0]]<<endl;
    int j = 1;
    for (i = 1; *s && i < oc; s++, j++) {
        c = code[(int)*s];
        // cerr<<"c: "<<c<<", ";
        if (out[i-1] == c + '0' || (i == 1 && j < 3 && (int)code[(int)out[0]] == c) || c < 1) {
            continue;
        }

        out[i++] = c + '0';
    }
    while (i < oc) out[i++] = '0';
    return out;
}

std::string
soundex(std::string const& s, int len = 4) {
    char *out = (char*)malloc(len + 1); 
    std::string tmp;
    tmp.reserve(s.size());

    // Remove non-ASCII characters
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] < 128) {
            tmp += s[i];
        }
    }

    c_soundex(tmp.c_str(), out, len);
    std::string ret(out);
    free(out);
    return ret;
}

namespace _soundex {
    int
    test() {
	int i;
	char sdx[5] = { 0 };
        const char *names[][2] = {
            {"Soundex",	"S532"},
            {"Example",	"E251"},
            {"Sownteks",	"S532"},
            {"Ekzampul",	"E251"},
            {"Euler",	"E460"},
            {"Gauss",	"G200"},
            {"Hilbert",	"H416"},
            {"Knuth",	"K530"},
            {"Lloyd",	"L300"},
            // {"Lukasiewicz",	"L202"},
            {"Lukasiewicz",	"L200"},
            {"Ellery",	"E460"},
            {"Ghosh",	"G200"},
            {"Heilbronn",	"H416"},
            {"Kant",	"K530"},
            {"Ladd",	"L300"},
            // {"Lissajous",	"L222"},
            {"Lissajous",	"L200"},
            {"Wheaton",	"W350"},
            {"Burroughs",	"B620"},
            {"Burrows",	"B620"},
            {"O'Hara",	"O600"},
            {"Washington",	"W252"},
            {"Lee",		"L000"},
            {"Gutierrez",	"G362"},
            {"Pfister",	"P236"},
            {"Jackson",	"J250"},
            // {"Tymczak",	"T522"},
            {"Tymczak",	"T520"},
            {"VanDeusen",	"V532"},
            {"Ashcraft",	"A261"},
            {"XPfizer", "X126"}, 
            {"XFizer", "X126"}, 
            {"XSandesh", "X532"}, 
            {"XSondesh", "X532"}, 
            {0, 0}
        };

        init();
        
        puts("  Test name  Code  Got\n----------------------");
        for (i = 0; names[i][0]; i++) {
            c_soundex(names[i][0], sdx, 4);
            printf("%11s  %s  %s ", names[i][0], names[i][1], sdx);
            printf("%s\n", strcmp(sdx, names[i][1]) ? "not ok" : "ok");
            assert(!strcmp(sdx, names[i][1]));
        }

        std::string s1 = "Xwholetthedogsout"; std::string s2 = "Xwholatethedocsout";
        printf("soundex(%s, %s) = (%s, %s)\n", s1.c_str(), s2.c_str(), soundex(s1, 7).c_str(), soundex(s2, 7).c_str());

        // ltdgst, ltdcst
        // 433223, 433223
        // 4323, 4323

        return 0;
    }
}
