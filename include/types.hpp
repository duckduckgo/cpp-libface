#if !defined LIBFACE_TYPES_HPP
#define LIBFACE_TYPES_HPP

#include <utility>
#include <vector>
#include <string>
#include <string.h>

#if !defined RMQ
#define RMQ SegmentTree
// #define RMQ SparseTable
#endif

typedef unsigned int uint_t;

struct StringProxy {
    const char *mem_base;
    int len;

    StringProxy(const char *_mb = NULL, int _l = 0)
        : mem_base(_mb), len(_l)
    { }

    void
    assign(const char *_mb, int _l) {
        this->mem_base = _mb;
        this->len = _l;
    }

    size_t
    size() const {
        return this->len;
    }

    operator std::string() const {
        return std::string(this->mem_base, this->len);
    }

    void
    swap(StringProxy &rhs) {
        std::swap(this->mem_base, rhs.mem_base);
        std::swap(this->len, rhs.len);
    }

};


struct phrase_t {
    uint_t weight;
    std::string phrase;
    StringProxy snippet;

    phrase_t(uint_t _w, std::string const& _p, StringProxy const& _s)
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
typedef std::vector<vpui_t> vvpui_t;

typedef std::pair<std::string, uint_t> psui_t;
typedef std::vector<psui_t> vpsui_t;
typedef std::vector<std::string> vs_t;
typedef std::pair<vs_t::iterator, vs_t::iterator> pvsi_t;
typedef std::pair<vpsui_t::iterator, vpsui_t::iterator> pvpsuii_t;


#endif // LIBFACE_TYPES_HPP
