#if !defined LIBFACE_SEGTREE_HPP
#define LIBFACE_SEGTREE_HPP


#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <stdio.h>
#include <assert.h>

#include <include/types.hpp>
#include <include/utils.hpp>

using namespace std;


class SegmentTree {
    vpui_t repr;
    uint_t len;
    // For each element, first is the max. value under (and including
    // this node) and second is the index where this max. value occurs.

public:
    SegmentTree(uint_t _len = 0) {
        this->initialize(_len);
    }

    void
    initialize(uint_t _len) {
        len = _len;
        this->repr.clear();
        this->repr.resize(1 << (log2(_len) + 2));
    }

    void initialize(vui_t const& elems) {
        if (elems.empty()) {
            this->initialize(0);
        }
        else {
            this->initialize(elems.size());
            this->_init(0, 0, elems.size() - 1, elems);
        }
    }

    pui_t
    _init(uint_t ni, uint_t b, uint_t e, vui_t const& elems) {
        // printf("ni: %u, b: %u, e: %u, size: %u\n", ni, b, e, elems.size());

        uint_t m = b + (e-b) / 2;
        if (b == e) {
            this->repr[ni] = pui_t(elems[b], b);
            // printf("Returning: [%u, %u]\n", this->repr[ni].first, this->repr[ni].second);
            return this->repr[ni];
        }
        else {
            pui_t lhs = this->_init(ni*2 + 1, b, m, elems);
            pui_t rhs = this->_init(ni*2 + 2, m+1, e, elems);
            return this->repr[ni] = (lhs.first > rhs.first ? lhs : rhs);
        }
    }

    pui_t
    _query_max(uint_t ni, uint_t b, uint_t e, uint_t qf, uint_t ql) {
        // printf("_query_max(%u, %u, %u, %u, %u)\n", ni, b, e, qf, ql);
        if (b > e || qf > e || ql < b) {
            // printf("[1] Returning: (-1, -1)\n");
            return pui_t(minus_one, minus_one);
        }

        if (b >= qf && e <= ql) {
            // printf("[2] Returning: (%d, %d)\n", this->repr[ni].first, this->repr[ni].second);
            return this->repr[ni];
        }

        uint_t m = b + (e-b) / 2;
        pui_t lhs = this->_query_max(ni*2 + 1, b, m, qf, ql);
        pui_t rhs = this->_query_max(ni*2 + 2, m+1, e, qf, ql);

        // printf("lhs.second is minus_one: %d, rhs.second is minus_one: %d\n", lhs.second == minus_one, rhs.second == minus_one);

        if (lhs.second == minus_one) {
            // printf("[3] Returning: (%d, %d)\n", rhs.first, rhs.second);
            return rhs;
        }
        else if (rhs.second == minus_one) {
            // printf("[4] Returning: (%d, %d)\n", lhs.first, lhs.second);
            return lhs;
        }
        else {
            pui_t &tmp = lhs.first > rhs.first ? lhs : rhs;
            // printf("[5] Returning: (%d, %d)\n", tmp.first, tmp.second);
            return tmp;
        }

    }

    // qf & ql are indexes; both inclusive.
    // first -> value, second -> index
    pui_t
    query_max(uint_t qf, uint_t ql) {
        return this->_query_max(0, 0, this->len - 1, qf, ql);
    }

};




namespace segtree {

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

        SegmentTree st;
        st.initialize(v);

	printf("Testing SegmentTree implementation\n");
	printf("----------------------------------\n");

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

#endif // LIBFACE_SEGTREE_HPP
