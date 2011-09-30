#if !defined LIBFACE_TYPES_HPP
#define LIBFACE_TYPES_HPP

#include <utility>
#include <vector>
#include <string>

typedef unsigned int uint_t;

struct phrase_t {
    uint_t weight;
    std::string phrase;
    std::string snippet;

    phrase_t(uint_t _w, std::string const& _p, std::string const& _s)
        : weight(_w), phrase(_p), snippet(_s) {
    }

    void
    swap(phrase_t& rhs) {
        std::swap(this->weight, rhs.weight);
        this->phrase.swap(rhs.phrase);
        this->snippet.swap(rhs.snippet);
    }

    bool
    operator<(phrase_t const& rhs) const {
        return this->phrase < rhs.phrase;
    }
};

// Specialize std::swap for our type
namespace std {
    void
    swap(phrase_t& lhs, phrase_t& rhs) {
        lhs.swap(rhs);
    }
}

typedef std::vector<phrase_t> vp_t;
typedef vp_t::iterator vpi_t;
typedef std::pair<vpi_t, vpi_t> pvpi_t;

typedef std::pair<uint_t, uint_t> pui_t;
typedef std::vector<uint_t> vui_t;
typedef std::vector<pui_t> vpui_t;

typedef std::pair<std::string, uint_t> psui_t;
typedef std::vector<psui_t> vpsui_t;
typedef std::vector<std::string> vs_t;
typedef std::pair<vs_t::iterator, vs_t::iterator> pvsi_t;
typedef std::pair<vpsui_t::iterator, vpsui_t::iterator> pvpsuii_t;


#endif // LIBFACE_TYPES_HPP
