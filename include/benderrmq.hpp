// -*- mode:c++; c-basic-offset:4 -*-
#if !defined LIBFACE_BENDERRMQ_HPP
#define LIBFACE_BENDERRMQ_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <stack>
#include <stdio.h>
#include <sstream>
#include <assert.h>

#include <include/types.hpp>
#include <include/utils.hpp>
#include <include/sparsetable.hpp>

using namespace std;

#define MIN_SIZE_FOR_BENDER_RMQ 16

std::string
bitmap_str(uint_t i) {
    std::string out;
    for (int x = 17; x >= 0; --x) {
	out += '0' + ((i & (1L << x)) ? 1 : 0);
    }
    return out;
}

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

template <typename T>
class SimpleFixedObjectAllocator {
    T *memory;
    uint_t n;
    uint_t start;

public:
    SimpleFixedObjectAllocator(uint_t _n)
        : memory(NULL), n(_n), start(0) {
        memory = (T*)operator new(sizeof(T) * n);
    }

    T *get() {
        assert_lt(start, n);
        return memory + (start++);
    }

    void put(T *mem) { }

    void clear() {
        operator delete(memory);
        memory = NULL;
    }

    ~SimpleFixedObjectAllocator() {
        clear();
    }
};


/* This is a destructive function - one which deletes the tree rooted
 * at node n
 */
void
euler_tour(BinaryTreeNode *n, 
	   vui_t &output, /* Where the output is written. Should be empty */
	   vui_t &levels, /* Where the level for each node is written. Should be empty */
	   vui_t &mapping /* mapping stores representative
                             indexes which maps from the original index to the index
                             into the euler tour array, which is a +- RMQ */, 
	   vui_t &rev_mapping /* Reverse mapping to go from +-RMQ
				 indexes to user provided indexes */, 
	   int level = 1) {
    DPRINTF("euler_tour(%d, %d)\n", n?n->data:-1, n?n->index:-1);
    if (!n) {
	return;
    }
    output.push_back(n->data);
    mapping[n->index] = output.size() - 1;
    DPRINTF("mapping[%d] = %d\n", n->index, mapping[n->index]);
    rev_mapping.push_back(n->index);
    levels.push_back(level);
    if (n->left) {
	euler_tour(n->left, output, levels, mapping, rev_mapping, level+1);
	output.push_back(n->data);
	rev_mapping.push_back(n->index);
	levels.push_back(level);
    }
    if (n->right) {
	euler_tour(n->right, output, levels, mapping, rev_mapping, level+1);
	output.push_back(n->data);
	rev_mapping.push_back(n->index);
	levels.push_back(level);
    }
    // We don't delete the node here since the clear() function on the
    // SimpleFixedObjectAllocator<BinaryTreeNode> will take care of
    // cleaning up the associated memory.
    //
    // delete n;
}

BinaryTreeNode*
make_cartesian_tree(vui_t const &input, SimpleFixedObjectAllocator<BinaryTreeNode> &alloc) {
    BinaryTreeNode *curr = NULL;
    std::stack<BinaryTreeNode*> stk;

    if (input.empty()) {
	return NULL;
    }

    for (uint_t i = 0; i < input.size(); ++i) {
	curr = alloc.get();
        new (curr) BinaryTreeNode(input[i], i);
	DPRINTF("ct(%d, %d)\n", curr->data, curr->index);

	if (stk.empty()) {
	    stk.push(curr);
	    DPRINTF("[1] stack top (%d, %d)\n", curr->data, curr->index);
	} else {
	    if (input[i] <= stk.top()->data) {
		// Just add it
		stk.push(curr);
		DPRINTF("[2] stack top (%d, %d)\n", curr->data, curr->index);
	    } else {
		// Back up till we are the largest node on the stack
		BinaryTreeNode *top = NULL;
		BinaryTreeNode *prev = NULL;
		while (!stk.empty() && stk.top()->data < input[i]) {
		    prev = top;
		    top = stk.top();
		    DPRINTF("[1] popping & setting (%d, %d)->right = (%d, %d)\n", top->data, top->index, 
			    prev?prev->data:-1, prev?prev->index:-1);
		    top->right = prev;
		    stk.pop();
		}
		curr->left = top;
		DPRINTF("(%d, %d)->left = (%d, %d)\n", curr->data, curr->index, top->data, top->index);
		stk.push(curr);
		DPRINTF("stack top is now (%d, %d)\n", curr->data, curr->index);
	    }
	}
    }

    assert(!stk.empty());
    BinaryTreeNode *top = NULL;
    BinaryTreeNode *prev = NULL;
    while (!stk.empty()) {
	prev = top;
	top = stk.top();
	DPRINTF("[2] popping & setting (%d, %d)->right = (%d, %d)\n", top->data, top->index, 
		prev?prev->data:-1, prev?prev->index:-1);
	top->right = prev;
	stk.pop();
    }
    DPRINTF("returning top = (%d, %d)\n", top->data, top->index);

    return top;
}

std::string
toGraphViz(BinaryTreeNode* par, BinaryTreeNode *n) {
    if (!n) {
	return "";
    }

    std::ostringstream sout;

    if (par) {
	sout<<'"'<<par->data<<"_"<<par->index<<"\" -> \""<<n->data<<"_"<<n->index<<"\"\n";
    }
    sout<<toGraphViz(n, n->left);
    sout<<toGraphViz(n, n->right);
    return sout.str();
}

class LookupTables {
    char_array_3d_t repr;

    /* The bitmaps are stored with the LSB always set as 1 and the LSB
     * signifying the sign of the element at array index 0. The MSB
     * represents the sign of the last element in the array relative
     * to the immediately previous element.
     */
public:
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
	    char_array_2d_t table(nbits-1, vc_t(nbits, -1));

	    // Compute the lookup table for the bitmap in 'i'.
	    for (int r = 0; r < nbits-1; ++r) {
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

    /* Return the index of the largest element in the range [l..u]
     * (both inclusive) within the lookup table at index 'index'.
     */
    uint_t
    query_max(uint_t index, uint_t l, uint_t u) {
	assert_le(l, u);
	assert_lt(index, repr.size());
	assert_lt(l, repr[0].size() + 1);
	assert_lt(u, repr[0][0].size());

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
	for (uint_t i = 0; i < repr.size(); ++i) {
	    DPRINTF("Bitmap: %s\n", bitmap_str(i).c_str());
	    DPRINTF("   |");

	    for (int c = 0; c < nc; ++c) {
		DPRINTF("%3d ", c);
		if (c+1 != nc) {
		    DPRINTF("|");
		}
	    }
	    DPRINTF("\n");
	    for (int r = 0; r < nr; ++r) {
		DPRINTF("%2d |", r);
		for (int c = 0; c < nc; ++c) {
		    DPRINTF("%3d ", (r<c ? repr[i][r][c] : -1));
		    if (c+1 != nc) {
			DPRINTF("|");
		    }
		}
		DPRINTF("\n");
	    }
	}
    }

};

class BenderRMQ {
    /* For inputs < MIN_SIZE_FOR_BENDER_RMQ in size, we use just the
     * sparse table.
     *
     * For inputs > MIN_SIZE_FOR_BENDER_RMQ in size, we use perform a
     * Euler Tour of the input and potentially blow it up to 2x. Let
     * the size of the blown up input be 'n' elements. We use 'st' for
     * 2n/lg n of the elements and for each block of size (1/2)lg n,
     * we use 'lt'. Since 'n' can be at most 2^32, (1/2)lg n can be at
     * most 16.
     *
     */
    SparseTable st;
    LookupTables lt;

    /* The data after euler tour computation (for +-RMQ) */
    vui_t euler;

    /* mapping stores the mapping of original indexes to indexes
     * within our re-written (using euler tour) structure).
     */
    vui_t mapping;

    /* Stores the bitmask corresponding to a block of size (1/2)(lg n) */
    vui_t table_map;

    /* Stores the mapping from +-RMQ indexes to actual indexes */
    vui_t rev_mapping;

    /* The real length of input that the user gave us */
    uint_t len;

    int lgn_by_2;
    int _2n_lgn;

public:

    void initialize(vui_t const& elems) {
	len = elems.size();

	if (len < MIN_SIZE_FOR_BENDER_RMQ) {
	    st.initialize(elems);
	    return;
	}

	vui_t levels;
        SimpleFixedObjectAllocator<BinaryTreeNode> alloc(len);

	euler.reserve(elems.size() * 2);
	mapping.resize(elems.size());
	BinaryTreeNode *root = make_cartesian_tree(elems, alloc);

	DPRINTF("GraphViz (paste at: http://ashitani.jp/gv/):\n%s\n", toGraphViz(NULL, root).c_str());

	euler_tour(root, euler, levels, mapping, rev_mapping);

	root = NULL; // This tree has now been deleted
        alloc.clear();

	assert_eq(levels.size(), euler.size());
	assert_eq(levels.size(), rev_mapping.size());

	uint_t n = euler.size();
	lgn_by_2 = log2(n) / 2;
	_2n_lgn  = n / lgn_by_2 + 1;

	DPRINTF("n = %u, lgn/2 = %d, 2n/lgn = %d\n", n, lgn_by_2, _2n_lgn);
	lt.initialize(lgn_by_2);

	table_map.resize(_2n_lgn);
	vui_t reduced;

	for (uint_t i = 0; i < n; i += lgn_by_2) {
	    uint_t max_in_block = euler[i];
	    int bitmap = 1L;
	    DPRINTF("Sequence: (%u, ", euler[i]);
	    for (int j = 1; j < lgn_by_2; ++j) {
		int curr_level, prev_level;
		uint_t value;
		if (i+j < n) {
		    curr_level = levels[i+j];
		    prev_level = levels[i+j-1];
		    value = euler[i+j];
		} else {
		    curr_level = 1;
		    prev_level = 0;
		    value = 0;
		}

		const uint_t bit = (curr_level < prev_level);
		bitmap |= (bit << j);
		max_in_block = std::max(max_in_block, value);
		DPRINTF("%u, ", value);
	    }
	    DPRINTF("), Bitmap: %s\n", bitmap_str(bitmap).c_str());
	    table_map[i / lgn_by_2] = bitmap;
	    reduced.push_back(max_in_block);
	}

        DPRINTF("reduced.size(): %u\n", reduced.size());
	st.initialize(reduced);
	DCERR("initialize() completed"<<endl);
    }

    // qf & ql are indexes; both inclusive.
    // Return: first -> value, second -> index
    pui_t
    query_max(uint_t qf, uint_t ql) {
        if (qf >= this->len || ql >= this->len || ql < qf) {
            return make_pair(minus_one, minus_one);
        }

	if (len < MIN_SIZE_FOR_BENDER_RMQ) {
            return st.query_max(qf, ql);
        }

	DPRINTF("[1] (qf, ql) = (%d, %d)\n", qf, ql);
	// Map to +-RMQ co-ordinates
	qf = mapping[qf];
	ql = mapping[ql];
	DPRINTF("[2] (qf, ql) = (%d, %d)\n", qf, ql);

	if (qf > ql) {
	    std::swap(qf, ql);
	    DPRINTF("[3] (qf, ql) = (%d, %d)\n", qf, ql);
	}

	// Determine whether we need to query 'st'.
	const uint_t first_block_index = qf / lgn_by_2;
	const uint_t last_block_index = ql / lgn_by_2;

	DPRINTF("first_block_index: %u, last_block_index: %u\n", 
		first_block_index, last_block_index);

	pui_t ret(0, 0);

        /* Main logic:
         *
         * [1] If the query is restricted to a single block, then
         * first_block_index == last_block_index, and we only need to
         * do a bitmap based lookup.
         *
         * [2] If last_block_index - first_block_index == 1, then the
         * query spans 2 blocks, and we do not need to lookup the
         * Sparse Table to get the summary max.
         *
         * [3] In all other cases, we need to take the maximum of 3
         * results, (a) the max in the suffix of the first block, (b)
         * the max in the prefix of the last block, and (c) the max of
         * all blocks between the first and the last block.
         *
         */

	if (last_block_index - first_block_index > 1) {
	    // We need to perform an inter-block query using the 'st'.
	    ret = st.query_max(first_block_index + 1, last_block_index - 1);

	    // Now perform an in-block query to get the index of the
	    // max value as it appears in 'euler'.
	    const uint_t bitmapx = table_map[ret.second];
	    const uint_t imax = lt.query_max(bitmapx, 0, lgn_by_2-1) + ret.second*lgn_by_2;
	    ret.second = imax;
	} else if (first_block_index == last_block_index) {
	    // The query is completely within a block.
	    const uint_t bitmapx = table_map[first_block_index];
	    DPRINTF("bitmapx: %s\n", bitmap_str(bitmapx).c_str());
	    qf %= lgn_by_2;
	    ql %= lgn_by_2;
	    const uint_t imax = lt.query_max(bitmapx, qf, ql) + first_block_index*lgn_by_2;
	    ret = make_pair(euler[imax], rev_mapping[imax]);
	    return ret;
	}

	// Now perform an in-block query for the first and last
	// blocks.
        const uint_t f1 = qf % lgn_by_2;
        const uint_t f2 = lgn_by_2 - 1;

        const uint_t l1 = 0;
        const uint_t l2 = ql % lgn_by_2;

        const uint_t bitmap1 = table_map[first_block_index];
        const uint_t bitmap2 = table_map[last_block_index];

	DPRINTF("bitmap1: %s, bitmap2: %s\n", bitmap_str(bitmap1).c_str(),
		bitmap_str(bitmap2).c_str());

	uint_t max1i = lt.query_max(bitmap1, f1, f2);
	uint_t max2i = lt.query_max(bitmap2, l1, l2);

	DPRINTF("max1i: %u, max2i: %u\n", max1i, max2i);

	max1i += first_block_index * lgn_by_2;
	max2i += last_block_index * lgn_by_2;

	if (last_block_index - first_block_index > 1) {
	    // 3-way max
	    DPRINTF("ret: %u, max1: %u, max2: %u\n", ret.first, euler[max1i], euler[max2i]);
	    if (ret.first > euler[max1i] && ret.first > euler[max2i]) {
		ret.second = rev_mapping[ret.second];
	    } else if (euler[max1i] >= ret.first && euler[max1i] >= euler[max2i]) {
		ret = make_pair(euler[max1i], rev_mapping[max1i]);
	    } else if (euler[max2i] >= ret.first && euler[max2i] >= euler[max1i]) {
		ret = make_pair(euler[max2i], rev_mapping[max2i]);
	    }
	} else {
	    // 2-way max
	    if (euler[max1i] > euler[max2i]) {
		ret = make_pair(euler[max1i], rev_mapping[max1i]);
	    } else {
		ret = make_pair(euler[max2i], rev_mapping[max2i]);
	    }
	}

	return ret;
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

	LookupTables lt;
	lt.initialize(4);
	// lt.show_tables();
	// return 0;

        vui_t v;
        v.push_back(45);
        v.push_back(4);
        v.push_back(5);
        v.push_back(2);
        v.push_back(99);
        v.push_back(41);
        v.push_back(45);
        v.push_back(47);
        v.push_back(51);
        v.push_back(89);
        v.push_back(1);
        v.push_back(3);
        v.push_back(5);
        v.push_back(98);
        v.push_back(289);
        v.push_back(14);
        v.push_back(95);
        v.push_back(88);

	BenderRMQ brmq;
        brmq.initialize(v);

        for (size_t i = 0; i < v.size(); ++i) {
            for (size_t j = i; j < v.size(); ++j) {
                pui_t one = brmq.query_max(i, j);
                pui_t two = naive_query_max(v, i, j);
                printf("query_max(%u, %u) == (%u, %u)\n", (uint_t)i, (uint_t)j, one.first, two.first);
                assert_eq(one.first, two.first);
            }
        }

	printf("\n");
        return 0;
    }
}

#endif // LIBFACE_BENDERRMQ_HPP
