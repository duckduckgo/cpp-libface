#if !defined LIBFACE_PHRASE_MAP_HPP
#define LIBFACE_PHRASE_MAP_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <assert.h>

using namespace std;

typedef unsigned int uint_t;
typedef pair<uint_t, uint_t> pui_t;
typedef vector<uint_t> vui_t;
typedef vector<pui_t> vpui_t;

typedef std::vector<std::string> vs_t;
typedef std::pair<vs_t::iterator, vs_t::iterator> pvsi_t;


class PrefixFinder {
    std::string prefix;

public:
    PrefixFinder(std::string p)
        : prefix(p)
    { }

    bool
    operator()(std::string const& lhs, std::string const &rhs) {
        std::string const *pprefix = &lhs;
        std::string const *ptarget = &rhs;

        bool rev = false;
        if (*ptarget == this->prefix) {
            std::swap(pprefix, ptarget);
            rev = true;
        }

        int ppos = ptarget->find(this->prefix);
        if (!ppos) {
            return false;
        }

        return lhs < rhs;
    }
};

class PhraseMap {
public:
    vs_t repr;

public:
    PhraseMap(uint_t _len = 10000) {
        this->repr.reserve(_len);
    }

    void
    insert(std::string const& str) {
        this->repr.push_back(str);
    }

    void
    finalize() {
        std::sort(this->repr.begin(), this->repr.end());
    }


    pvsi_t
    query(std::string const &prefix) {
        return std::equal_range(this->repr.begin(), this->repr.end(), 
                                prefix, PrefixFinder(prefix));
    }

};


pvsi_t
naive_query(PhraseMap &pm, std::string prefix) {
    vs_t::iterator f = pm.repr.begin(), l = pm.repr.begin();
    while (f != pm.repr.end() && f->substr(0, prefix.size()) < prefix) {
        ++f;
    }
    l = f;
    while (l != pm.repr.end() && l->substr(0, prefix.size()) == prefix) {
        ++l;
    }
    return std::make_pair(f, l);
}

void
show_indexes(PhraseMap &pm, std::string prefix) {
    pvsi_t nq = naive_query(pm, prefix);
    pvsi_t q  = pm.query(prefix);

    cout<<"naive[first] = "<<nq.first - pm.repr.begin()<<", naive[last] = "<<nq.second - pm.repr.begin()<<endl;
    cout<<"phmap[first] = "<<q.first - pm.repr.begin()<<", phmap[last] = "<<q.second - pm.repr.begin()<<endl;
}


namespace phrase_map {
    int
    test() {
        PhraseMap pm;
        pm.insert("duckduckgo");
        pm.insert("duckduckgeese");
        pm.insert("duckduckgoose");
        pm.insert("duckduckgoo");
        pm.insert("duckgo");
        pm.insert("dukgo");
        pm.insert("luckkuckgo");
        pm.insert("chuckchuckgo");
        pm.insert("dilli - no one killed jessica");
        pm.insert("aaitbaar - no one killed jessica");

        pm.finalize();

        show_indexes(pm, "a");
        assert(naive_query(pm, "a") == pm.query("a"));

        show_indexes(pm, "b");
        assert(naive_query(pm, "b") == pm.query("b"));

        show_indexes(pm, "c");
        assert(naive_query(pm, "c") == pm.query("c"));

        show_indexes(pm, "d");
        assert(naive_query(pm, "d") == pm.query("d"));

        show_indexes(pm, "duck");
        assert(naive_query(pm, "duck") == pm.query("duck"));

        show_indexes(pm, "ka");
        assert(naive_query(pm, "ka") == pm.query("ka"));

        return 0;
    }
}

#endif // LIBFACE_PHRASE_MAP_HPP
