#if !defined LIBFACE_UTILS_HPP
#define LIBFACE_UTILS_HPP

#include <vector>
#include <utility>
#include <ostream>

template <typename T>
ostream&
operator<<(ostream& out, vector<T> const& vec) {
    for (size_t i = 0; i < vec.size(); ++i) {
        out<<vec[i]<<endl;
    }
    return out;
}

template <typename T, typename U>
ostream&
operator<<(ostream& out, std::pair<T, U> const& p) {
    out<<"("<<p.first<<", "<<p.second<<")";
    return out;
}


#endif // LIBFACE_UTILS_HPP
