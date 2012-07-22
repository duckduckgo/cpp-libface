// -*- mode:c++; c-basic-offset:4 -*-
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
    /* For each element in repr, we store just the index of the MAX
     * element in data.
     *
     * i.e. If the MAX element in the index range [10..17] is at index
     * 14, then repr[3][10] will contain the value 14.
     *
     * repr[X] stores MAX indexes for blocks of length (1<<X == 2^X).
     *
     */
    vui_t data;
    vvui_t repr;
    uint_t len;

public:

    void initialize(vui_t const& elems) {

	this->data = elems;
        this->len = elems.size();
        this->repr.clear();

        DCERR("len: "<<this->len<<endl);

        const size_t ntables = log2(this->len) + 1;
        this->repr.resize(ntables);

        DCERR("ntables: "<<ntables<<endl);

        this->repr[0].resize(this->len);
        for (size_t i = 0; i < this->len; ++i) {
	    // This is the identity mapping, since the MAX element in
	    // a range of length 1 is the element itself.
            this->repr[0][i] = i;
        }

        for (size_t i = 1; i < ntables; ++i) {
	    /* The previous 'block size' */
            const uint_t pbs = 1<<(i-1);

            /* bs is the 'block size'. i.e. The number of elements
	     * from the data that are used to computed the max value
	     * and store it at repr[i][...].
	     */
            const uint_t bs = 1<<i;

	    /* The size of the vector at repr[i]. We need to resize it
	     * to this size.
	     */
            const size_t vsz = this->len - bs + 1;

	    DCERR("starting i: "<<i<<" bs: "<<bs<<endl);

            this->repr[i].resize(vsz);

            // cerr<<"i: "<<i<<", vsz: "<<vsz<<endl;

            vui_t& curr = this->repr[i];
            vui_t& prev = this->repr[i - 1];

            for (size_t j = 0; j < vsz; ++j) {
                // 'j' is the starting index of a block of size 'bs'
		const uint_t prev_elem1 = data[prev[j]];
		const uint_t prev_elem2 = data[prev[j+pbs]];
                if (prev_elem1 > prev_elem2) {
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
	const uint_t data1 = data[this->repr[ti][f]];
	const uint_t data2 = data[this->repr[ti][l]];

        if (data1 > data2) {
            return std::make_pair(data1, this->repr[ti][f]);
        }
        else {
            return std::make_pair(data2, this->repr[ti][l]);
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
	printf("Testing SparseTable implementation\n");
	printf("----------------------------------\n");

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
                assert_eq(one.first, two.first);
            }
        }

	printf("\n");
        return 0;
    }
}

#endif // LIBFACE_SPARSETABLE_HPP
