#if !defined LIBFACE_SPARSETABLE_HPP
#define LIBFACE_SPARSETABLE_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <stdio.h>
#include <assert.h>

#include <include/types.hpp>
#include <include/utils.hpp>

using namespace std;


class SparseTable {
    vvpui_t repr;
    uint_t len;
    // For each element, first is the max. value under (and including
    // this node) and second is the index where this max. value occurs.

public:

    void initialize(vui_t const& elems) {

        this->len = elems.size();
        this->repr.clear();

        cerr<<"len: "<<this->len<<endl;

        const size_t ntables = log2(this->len) + 1;
        this->repr.resize(ntables);

        cerr<<"ntables: "<<ntables<<endl;

        this->repr[0].resize(this->len);
        for (size_t i = 0; i < this->len; ++i) {
            this->repr[0][i] = std::make_pair(elems[i], i);
        }

        for (size_t i = 1; i < ntables; ++i) {
            // bs is the 'block size'. i.e. Number of element in the
            // array this->repr[i]
            // cerr<<"starting i: "<<i<<endl;
            const uint_t pbs = 1<<(i-1);
            const uint_t bs = 1<<i;
            const size_t vsz = this->len - bs + 1;

            this->repr[i] = vpui_t();
            this->repr[i].resize(vsz);

            // cerr<<"i: "<<i<<", vsz: "<<vsz<<endl;

            vpui_t& curr = this->repr[i];
            vpui_t& prev = this->repr[i - 1];

            for (size_t j = 0; j < vsz; ++j) {
                // 'j' is the starting index of a block of size 'bs'
                if (prev[j].first > prev[j+pbs].first) {
                    curr[j] = prev[j];
                }
                else {
                    curr[j] = prev[j+pbs];
                }
                // cerr<<"curr["<<j<<"] = "<<curr[j].first<<endl;
            }
            // cerr<<"done with i: "<<i<<endl;
        }
        // cerr<<"initialize() completed"<<endl;
    }

    // qf & ql are indexes; both inclusive.
    // first -> value, second -> index
    pui_t
    query_max(uint_t qf, uint_t ql) {
        if (qf >= this->len || ql >= this->len || ql < qf) {
            return make_pair(minus_one, minus_one);
        }

        const int rlen = ql - qf + 1;
        const size_t ti = log2(rlen);
        const size_t f = qf, l = ql + 1 - (1 << ti);

        // cerr<<"query_max("<<qf<<", "<<ql<<"), ti: "<<ti<<", f: "<<f<<", l: "<<l<<endl;
        if (this->repr[ti][f].first > this->repr[ti][l].first) {
            return this->repr[ti][f];
        }
        else {
            return this->repr[ti][l];
        }
    }

};


namespace sparsetable {

    pui_t
    naive_query_max(vui_t const& v, int i, int j) {
        uint_t mv = v[i];
        uint_t mi = i;
        while (i <= j) {
            if (v[i] > mv) {
                mv = v[i];
                mi = i;
            }
            ++i;
        }
        return pui_t(mv, mi);
    }

    int
    test() {
        vui_t v;
        v.push_back(45);
        v.push_back(4);
        v.push_back(5);
        v.push_back(2);
        v.push_back(99);
        v.push_back(41);
        v.push_back(45);
        v.push_back(45);
        v.push_back(51);
        v.push_back(89);
        v.push_back(1);
        v.push_back(3);
        v.push_back(5);
        v.push_back(98);

        for (int i = 0; i < 10; ++i) {
            // printf("%d: %d\n", i, log2(i));
        }

        SparseTable st;
        st.initialize(v);

        for (size_t i = 0; i < v.size(); ++i) {
            for (size_t j = i; j < v.size(); ++j) {
                pui_t one = st.query_max(i, j);
                pui_t two = naive_query_max(v, i, j);
                printf("query_max(%u, %u) == (%u, %u)\n", (uint_t)i, (uint_t)j, one.first, two.first);
                assert(one.first == two.first);
            }
        }

        return 0;
    }
}

#endif // LIBFACE_SPARSETABLE_HPP
