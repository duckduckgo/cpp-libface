#include <include/segtree.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>
#include <include/sparsetable.hpp>

int
main() {
    segtree::test();
    sparsetable::test();
    phrase_map::test();
    _suggest::test();

    return 0;
}
