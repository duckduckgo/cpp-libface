#if !defined LIBFACE_TYPES_HPP
#define LIBFACE_TYPES_HPP

#include <utility>
#include <vector>
#include <string>

typedef unsigned int uint_t;
typedef std::pair<uint_t, uint_t> pui_t;
typedef std::vector<uint_t> vui_t;
typedef std::vector<pui_t> vpui_t;

typedef std::pair<std::string, uint_t> psui_t;
typedef std::vector<psui_t> vpsui_t;
typedef std::vector<std::string> vs_t;
typedef std::pair<vs_t::iterator, vs_t::iterator> pvsi_t;
typedef std::pair<vpsui_t::iterator, vpsui_t::iterator> pvpsuii_t;


#endif // LIBFACE_TYPES_HPP
