#include <include/sparsetable.hpp>
#include <include/segtree.hpp>
#include <include/benderrmq.hpp>
#include <include/phrase_map.hpp>
#include <include/suggest.hpp>
#include <include/soundex.hpp>
#include <include/editdistance.hpp>

int
main() {
    segtree::test();
    sparsetable::test();
    benderrmq::test();

    phrase_map::test();
    _soundex::test();
    editdistance::test();

    return 0;
}
