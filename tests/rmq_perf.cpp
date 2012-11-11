#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <iostream>

#include <include/sparsetable.hpp>
#include <include/segtree.hpp>
#include <include/benderrmq.hpp>
#include <include/types.hpp>
#include <include/utils.hpp>

using namespace std;

#if !defined NUM_ELEMS
#define NUM_ELEMS 8000000
#endif

#if !defined NUM_ITERATIONS
#define NUM_ITERATIONS 3000
#endif

#if !defined NUM_QUERIES
#define NUM_QUERIES 10000
#endif


void
test_segtree(vui_t &input, vpui_t &queries, vui_t &expected) {
    clock_t start, end;
    start = clock();

    SegmentTree st;
    st.initialize(input);

    end = clock();
    printf("Initialization time: %f sec\n", ((double)(end - start))/CLOCKS_PER_SEC);

    start = end;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        for (size_t j = 0; j < queries.size(); ++j) {
            pui_t result = st.query_max(queries[j].first, queries[j].second);
            assert_eq(result.first, input[expected[j]]);
            assert_eq(input[result.second], input[expected[j]]);
        }
    }

    end = clock();
    printf("Query time: %f sec\n\n", ((double)(end - start))/CLOCKS_PER_SEC);
}

void
test_sparsetable(vui_t &input, vpui_t &queries, vui_t &expected) {
    clock_t start, end;
    start = clock();

    SparseTable st;
    st.initialize(input);

    end = clock();
    printf("Initialization time: %f sec\n", ((double)(end - start))/CLOCKS_PER_SEC);

    start = end;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        for (size_t j = 0; j < queries.size(); ++j) {
            pui_t result = st.query_max(queries[j].first, queries[j].second);
            assert_eq(result.first, input[expected[j]]);
            assert_eq(input[result.second], input[expected[j]]);
        }
    }
    end = clock();
    printf("Query time: %f sec\n\n", ((double)(end - start))/CLOCKS_PER_SEC);
}

void
test_benderrmq(vui_t &input, vpui_t &queries, vui_t &expected) {
    clock_t start, end;
    start = clock();

    BenderRMQ brmq;
    brmq.initialize(input);

    end = clock();
    printf("Initialization time: %f sec\n", ((double)(end - start))/CLOCKS_PER_SEC);

    start = end;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        for (size_t j = 0; j < queries.size(); ++j) {
            pui_t result = brmq.query_max(queries[j].first, queries[j].second);
            assert_eq(result.first, input[expected[j]]);
            assert_eq(input[result.second], input[expected[j]]);
        }
    }
    end = clock();
    printf("Query time: %f sec\n\n", ((double)(end - start))/CLOCKS_PER_SEC);
}

uint_t
index_of_max_in_range(vui_t &input, uint_t i, uint_t j) {
    assert(i <= j);
    uint_t m = input[i];
    uint_t mi = i;
    while (++i <= j) {
        if (input[i] > m) {
            m = input[i];
            mi = i;
        }
    }
    return mi;
}

void setup_tests(vui_t &input, vpui_t &queries, vui_t &results) {
    input.resize(NUM_ELEMS);
    for (int i = 0; i < NUM_ELEMS; ++i) {
        input[i] = rand() % (RAND_MAX/1);
    }

    queries.resize(NUM_QUERIES);
    results.resize(NUM_QUERIES);
    for (int i = 0; i < NUM_QUERIES; ++i) {
        uint_t lb, ub;
        lb = rand() % NUM_ELEMS;
        ub = rand() % NUM_ELEMS;
        if (ub < lb) {
            std::swap(lb, ub);
        }
        queries[i] = std::make_pair(lb, ub);
        results[i] = index_of_max_in_range(input, lb, ub);
    }
}

int
main() {

    vui_t input;
    vpui_t queries;
    vui_t results;

    printf("Setting up test structures\n\n");
    setup_tests(input, queries, results);

    printf("Starting Sparse Table Test\n");
    test_sparsetable(input, queries, results);

    printf("Starting Segment Tree Test\n");
    test_segtree(input, queries, results);

    printf("Starting Bender RMQ Test\n");
    test_benderrmq(input, queries, results);
}
