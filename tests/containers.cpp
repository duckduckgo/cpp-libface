#if !defined RMQ
// #define RMQ SegmentTree
#define RMQ SparseTable
#endif

#include <include/sparsetable.hpp>
#include <include/segtree.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>

int
main() {
    segtree::test();
    sparsetable::test();
    phrase_map::test();
    _suggest::test();

    return 0;
}
