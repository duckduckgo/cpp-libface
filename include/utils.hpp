#if !defined LIBFACE_UTILS_HPP
#define LIBFACE_UTILS_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <ostream>
#include <assert.h>
#include <stdio.h>

#include <include/types.hpp>

#if defined DEBUG
#define DCERR(X) std::cerr<<X;
#define DPRINTF(ARGS...) fprintf(stderr, ARGS);
#else
#define DCERR(X)
#define DPRINTF(ARGS...)
#endif

#define assert_lt(X,Y) if (!((X)<(Y))) { fprintf(stderr, "%d < %d FAILED\n", (X), (Y)); assert((X)<(Y)); }
#define assert_gt(X,Y) if (!((X)>(Y))) { fprintf(stderr, "%d > %d FAILED\n", (X), (Y)); assert((X)>(Y)); }
#define assert_le(X,Y) if (!((X)<=(Y))) { fprintf(stderr, "%d <= %d FAILED\n", (X), (Y)); assert((X)<=(Y)); }
#define assert_eq(X,Y) if (!((X)==(Y))) { fprintf(stderr, "%d == %d FAILED\n", (X), (Y)); assert((X)==(Y)); }
#define assert_ne(X,Y) if (!((X)!=(Y))) { fprintf(stderr, "%d != %d FAILED\n", (X), (Y)); assert((X)!=(Y)); }

inline uint_t log2(uint_t n) {
    uint_t lg2 = 0;
    while (n > 1) {
        n /= 2;
        ++lg2;
    }
    return lg2;
}

const uint_t minus_one = (uint_t)0 - 1;

template <typename T>
std::ostream&
operator<<(std::ostream& out, std::vector<T> const& vec) {
    for (size_t i = 0; i < vec.size(); ++i) {
        out<<vec[i]<<std::endl;
    }
    return out;
}

template <typename T, typename U>
std::ostream&
operator<<(std::ostream& out, std::pair<T, U> const& p) {
    out<<"("<<p.first<<", "<<p.second<<")";
    return out;
}

inline std::ostream&
operator<<(std::ostream& out, phrase_t const& p) {
    out<<"("<<p.phrase<<", "<<p.weight<<")";
    return out;
}


#endif // LIBFACE_UTILS_HPP
