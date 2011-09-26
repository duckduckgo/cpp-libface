#if !defined LIBFACE_SUGGEST_HPP
#define LIBFACE_SUGGEST_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <string>
#include <queue>
#include <stdio.h>
#include <assert.h>

using namespace std;

typedef unsigned int uint_t;
typedef pair<uint_t, uint_t> pui_t;
typedef vector<uint_t> vui_t;
typedef vector<pui_t> vpui_t;

typedef std::pair<std::string, uint_t> psui_t;
typedef std::vector<psui_t> vpsui_t;
typedef std::vector<std::string> vs_t;
typedef std::pair<vs_t::iterator, vs_t::iterator> pvsi_t;
typedef std::pair<vpsui_t::iterator, vpsui_t::iterator> pvpsuii_t;


struct PhraseRange {
    // first & last are both inclusive of the range. i.e. The range is
    // [first, last] and NOT [first, last)
    uint_t first, last;

    // weight is the value of the best solution in the range [first,
    // last] and index is the index of this best solution in the
    // original array of strings.
    uint_t weight, index;

    PhraseRange(uint_t f, uint_t l, uint_t w, uint_t i)
        : first(f), last(l), weight(w), index(i)
    { }

    bool
    operator<(PhraseRange const &rhs) const {
        return this->weight < rhs.weight;
    }
};

typedef std::priority_queue<PhraseRange> pqpr_t;



vpsui_t
suggest(PhraseMap &pm, SegmentTree &st, std::string prefix, int n = 16) {
    pvpsuii_t phrases = pm.query(prefix);
    uint_t first = phrases.first - pm.repr.begin();
    uint_t last = phrases.second - pm.repr.begin();

    if (first == last) {
        return vpsui_t();
    }

    vpsui_t ret;
    --last;

    pqpr_t heap;
    pui_t best = st.query_max(first, last);
    heap.push(PhraseRange(first, last, best.first, best.second));

    while (ret.size() < n && !heap.empty()) {
        PhraseRange pr = heap.top();
        ret.push_back(pm.repr[pr.index]);

        uint_t lower = pr.first;
        uint_t upper = pr.index - 1;
        if (lower <= upper) {
            best = st.query_max(lower, upper);
            heap.push(PhraseRange(lower, upper, best.first, best.second));
        }

        lower = pr.index + 1;
        upper = pr.last;
        if (lower <= upper) {
            best = st.query_max(lower, upper);
            heap.push(PhraseRange(lower, upper, best.first, best.second));
        }
    }

    return ret;
}

vpsui_t
naive_suggest(PhraseMap &pm, SegmentTree &st, std::string prefix, int n = 16) {
    pvpsuii_t phrases = pm.query(prefix);
    std::vector<uint_t> indexes;
    vpsui_t ret;

    while (phrases.first != phrases.second) {
        indexes.push_back(phrases.first - pm.repr.begin());
        ++phrases.first;
    }

    while (ret.size() < n && !indexes.empty()) {
        uint_t mi = 0;
        for (size_t i = 1; i < indexes.size(); ++i) {
            if (pm.repr[i].second > pm.repr[mi].second) {
                mi = i;
            }
        }
        ret.push_back(pm.repr[indexes[mi]]);
        indexes.erase(indexes.begin() + mi);
    }
    return ret;
}

namespace _suggest {
    int
    test() {
        PhraseMap pm;
        pm.insert("duckduckgo", 1);
        pm.insert("duckduckgeese", 2);
        pm.insert("duckduckgoose", 1);
        pm.insert("duckduckgoo", 9);
        pm.insert("duckgo", 10);
        pm.insert("dukgo", 3);
        pm.insert("luckkuckgo", 2);
        pm.insert("chuckchuckgo", 5);
        pm.insert("dilli - no one killed jessica", 15);
        pm.insert("aaitbaar - no one killed jessica", 11);

        pm.finalize();

        SegmentTree st;
        vui_t weights;
        for (size_t i = 0; i < pm.repr.size(); ++i) {
            weights.push_back(pm.repr[i].second);
        }

        st.initialize(weights);

        assert(suggest(pm, st, "d") == naive_suggest(pm, st, "d"));

        assert(suggest(pm, st, "a") == naive_suggest(pm, st, "a"));

        return 0;
    }
}

#endif // LIBFACE_PHRASE_MAP_HPP
