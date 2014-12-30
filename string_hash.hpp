#ifndef STRING_HASH_HPP_
#define STRING_HASH_HPP_

#include <string>
#include <ext/hash_map>

namespace __gnu_cxx
{
    template<> struct hash<const std::string>
    {
        size_t operator()(const std::string& s) const
        { return hash<const char*>()( s.c_str() ); }  // __stl_hash_string
    };
    template<> struct hash<std::string>
    {
        size_t operator()(const std::string& s) const
        { return hash<const char*>()( s.c_str() ); }
    };
}

#endif  // STRING_HASH_HPP_
