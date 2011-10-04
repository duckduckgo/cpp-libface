#if !defined LIBFACE_UTILS_HPP
#define LIBFACE_UTILS_HPP

#include <vector>
#include <utility>
#include <ostream>

#include <include/types.hpp>

#if defined NDEBUG
#define DCERR(X)
#else
#define DCERR(X) cerr<<X;
#endif


uint_t log2(uint_t n) {
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

std::ostream&
operator<<(std::ostream& out, phrase_t const& p) {
    out<<"("<<p.phrase<<", "<<p.weight<<")";
    return out;
}


#endif // LIBFACE_UTILS_HPP
