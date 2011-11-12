#if !defined LIBFACE_PHRASE_MAP_HPP
#define LIBFACE_PHRASE_MAP_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <assert.h>

#include <include/types.hpp>

using namespace std;



struct PrefixFinder {
    bool
    operator()(std::string const& prefix, phrase_t const &target) {
#if 1
        const int ppos = target.phrase.compare(0, prefix.size(), prefix);
        return ppos > 0;
#else
        const int ppos = target.phrase.find(prefix);
        if (!ppos) {
            return false;
        }
        return prefix < target.phrase;
#endif
    }

    bool
    operator()(phrase_t const& target, std::string const &prefix) {
#if 1
        const int ppos = target.phrase.compare(0, prefix.size(), prefix);
        return ppos < 0;
#else
        const int ppos = target.phrase.find(prefix);
        if (!ppos) {
            return false;
        }
        return target.phrase < prefix;
#endif
    }
};

class PhraseMap {
public:
    vp_t repr;

public:
    PhraseMap(uint_t _len = 15000000) {
        this->repr.reserve(_len);
    }

    void
    insert(uint_t weight, std::string const& p, StringProxy const& s) {
        this->repr.push_back(phrase_t(weight, p, s));
    }

    void
    finalize(int sorted = 0) {
        if (!sorted) {
            std::sort(this->repr.begin(), this->repr.end());
        }
    }

    pvpi_t
    query(std::string const &prefix) {
        return std::equal_range(this->repr.begin(), this->repr.end(), 
                                prefix, PrefixFinder());
    }

};


pvpi_t
naive_query(PhraseMap &pm, std::string prefix) {
    vpi_t f = pm.repr.begin(), l = pm.repr.begin();
    while (f != pm.repr.end() && f->phrase.substr(0, prefix.size()) < prefix) {
        ++f;
    }
    l = f;
    while (l != pm.repr.end() && l->phrase.substr(0, prefix.size()) == prefix) {
        ++l;
    }
    return std::make_pair(f, l);
}

void
show_indexes(PhraseMap &pm, std::string prefix) {
    pvpi_t nq = naive_query(pm, prefix);
    pvpi_t q  = pm.query(prefix);

    cout<<"naive[first] = "<<nq.first - pm.repr.begin()<<", naive[last] = "<<nq.second - pm.repr.begin()<<endl;
    cout<<"phmap[first] = "<<q.first - pm.repr.begin()<<", phmap[last] = "<<q.second - pm.repr.begin()<<endl;
}


namespace phrase_map {
    int
    test() {
        PhraseMap pm;
        pm.insert(1, "duckduckgo", "");
        pm.insert(2, "duckduckgeese", "");
        pm.insert(1, "duckduckgoose", "");
        pm.insert(9, "duckduckgoo", "");
        pm.insert(10, "duckgo", "");
        pm.insert(3, "dukgo", "");
        pm.insert(2, "luckkuckgo", "");
        pm.insert(5, "chuckchuckgo", "");
        pm.insert(15, "dilli - no one killed jessica", "");
        pm.insert(11, "aaitbaar - no one killed jessica", "");

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
