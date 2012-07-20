// -*- mode:c++; c-basic-offset:4 -*-
#if !defined LIBFACE_BENDERRMQ_HPP
#define LIBFACE_BENDERRMQ_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <stdio.h>
#include <assert.h>

#include <include/types.hpp>
#include <include/utils.hpp>
#include <include/sparsetable.hpp>

using namespace std;

struct BinaryTreeNode {
    BinaryTreeNode *left, *right;
    uint_t data;
    uint_t index;

    BinaryTreeNode(BinaryTreeNode *_left, BinaryTreeNode *_right, uint_t _data, uint_t _index)
	: left(_left), right(_right), data(_data), index(_index)
    { }

    BinaryTreeNode(uint_t _data, uint_t _index)
	: left(NULL), right(NULL), data(_data), index(_index)
    { }
};

void
euler_tour(BinaryTreeNode *n, 
	   vui_t &output, 
	   vui_t &levels, 
	   vui_t &rep_indexes /* rep_indexes stores representative
	      indexes which maps from the original index to the index
	      into the euler tour array, which is a +- RMQ */, 
	   int level = 1) {
    if (!n) {
	return;
    }
    output.push_back(n->data);
    rep_indexes[n->index] = output.size() - 1;
    levels.push_back(level);
    if (n->left) {
	euler_tour(n->left, output, levels, rep_indexes, level+1);
	output.push_back(n->data);
    }
    if (n->right) {
	euler_tour(n->right, output, levels, rep_indexes, level+1);
	output.push_back(n->data);
    }
}

BinaryTreeNode*
make_cartesian_tree(vui_t const &input) {
    BinaryTreeNode *curr = NULL;
    std::stack<BinaryTreeNode*> stk;

    if (input.empty()) {
	return NULL;
    }

    for (int i = 0; i < input.size(); ++i) {
	curr = new BinaryTreeNode(input[i], i);
	if (stk.empty()) {
	    stk.push(curr);
	} else {
	    if (input[i] < stk.top()->data) {
		// Just add it
		stk.top()->right = curr;
		stk.push(curr);
	    } else {
		// Back up till we are the largest node on the stack
		BinaryTreeNode *top = NULL;
		while (!stk.empty() && stk.top()->data < input[i]) {
		    top = stk.top();
		    stk.pop();
		}
		top->left = curr;
		stk.push(curr);
	    }
	}
    }

    assert(!stk.empty());
    BinaryTreeNode *top = NULL;
    while (!stk.empty()) {
	top = stk.top();
	stk.pop();
    }
    return top;
}

class LookupTables {
    vvvc_t repr;

public:
    LookupTables() { }

    void initialize(int nbits) {
	int ntables = 1 << nbits;
	repr.resize(ntables);
	std::vector<int> tmp(nbits);

	for (int i = 0; i < ntables; ++i) {
	    const int start = 40;
	    int diff = 0;
	    for (int j = 0; j < nbits; ++j) {
		const int mask = 1L << j;
		if (i & mask) {
		    diff += 1;
		} else {
		    diff -= 1;
		}
		tmp[j] = start + diff;
	    }

	    // Always perform a lookup with the lower index
	    // first. i.e. table[3][5] and NOT table[5][3]. Never
	    // lookup with the same index on both dimenstion (for
	    // example: table[3][3]).
	    vvc_t table(nbits-1, vc_t(nbits));

	    // Compute the lookup table for the bitmap in 'i'.
	    for (int r = 0; r < nbits; ++r) {
		int maxi = r;
		int maxv = tmp[r];

		for (int c = r+1; c < nbits; ++c) {
		    if (tmp[c] > maxv) {
			maxv = tmp[c];
			maxi = c;
		    }
		    table[r][c] = maxi;

		} // for(c)

	    } // for (r)

	    repr[i].swap(table);

	} // for (i)
    }

    uint_t
    query_max(uint_t index, uint_t l, uint_t u) {
	assert(l <= u);
	assert(index < repr.size());

	if (u == l) {
	    return u;
	}

	return repr[index][l][u];
    }

    void
    show_tables() {
	if (repr.empty()) {
	    return;
	}
	int nr = repr[0].size();
	int nc = repr[0][0].size();
	for (int i = 0; i < repr.size(); ++i) {
	    printf("Bitmap: ");
	    for (int x = 17; x >= 0; --x) {
		printf("%d", (i & (1L << x)) ? 1 : 0);
	    }
	    printf("\n");

	    printf("   |");
	    for (int c = 0; c < nc; ++c) {
		printf("%3d ", c);
		if (c+1 != nc) {
		    printf("|");
		}
	    }
	    printf("\n");
	    for (int r = 0; r < nr; ++r) {
		printf("%2d |", r);
		for (int c = 0; c < nc; ++c) {
		    printf("%3d ", (r<c ? repr[i][r][c] : -1));
		    if (c+1 != nc) {
			printf("|");
		    }
		}
		printf("\n");
	    }
	}
    }

};

class BenderRMQ {
    /* For inputs <= 64 in size, we use just the sparse table.
     *
     * For inputs > 64 in size, we use perform a Euler Tour of the
     * input and potentially blow it up to 2x. Let the size of the
     * blown up input be 'n' elements. We use 'st' for 2n/lg n of the
     * elements and for each block of size (1/2)lg n, we use
     * 'lt'. Since 'n' can be at most 2^32, (1/2)lg n can be at most
     * 16.
     *
     */
    SparseTable st;
    LookupTables lt;

    /* mapping stores the mapping of original indexes to indexes
     * within our re-written (using euler tour) structure).
     */
    vpui_t mapping;

    /* The real length of input that the user gave us */
    uint_t len;

    int lgn_by_2;
    int _2n_lgn;

public:

    void initialize(vui_t const& elems) {
	len = elems.length();
	if (elems.size() < 15) {
	    st.initialize(elems);
	    return;
	}

	vui_t euler, levels;
	euler.reserve(elems.size() * 2);
	mapping.resize(elems.size());
	BinaryTreeNode *root = make_cartesian_tree(elems);
	euler_tour(root, euler, levels, mapping);

	unit_t n = euler.size();
	lgn_by_2 = log2(n) / 2;
	_2n_lgn  = n / lgn_by_2;
	lt.initialize(lgn_by_2);

	vui_t reduced;

	for (uint_t i = 0; i < n; i += lgn_by_2) {
	    reduced.push_back(euler[i]);
	    int bitmap = 1L;
	    for (int j = 1; j < lgn_by_2; ++j) {
		bitmap <<= 1;
		bitmap |= (levels[i] > levels[i-1]);
	    }
	}
	cerr<<"initialize() completed"<<endl;
    }

    // qf & ql are indexes; both inclusive.
    // first -> value, second -> index
    pui_t
    query_max(uint_t qf, uint_t ql) {
        if (qf >= this->len || ql >= this->len || ql < qf) {
            return make_pair(minus_one, minus_one);
        }

    }

};


namespace benderrmq {

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
	printf("Testing BenderRMQ implementation\n");
	printf("--------------------------------\n");

	LookupTables lt(4);
	lt.show_tables();

	/*
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

	printf("\n");
        return 0;
	*/

    }
}

#endif // LIBFACE_SPARSETABLE_HPP
