#include <string>
#include <ext/hash_map>
#include "string_hash.hpp"

int main() {
  __gnu_cxx::hash_map<std::string, void*> map;
  std::string str;
  __gnu_cxx::hash_map<std::string, void*>::iterator uriItr = map.find(str);
}
