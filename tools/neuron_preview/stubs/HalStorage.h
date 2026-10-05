#pragma once
// Host preview: the "SD card" is the directory in $NEURON_SD (default ./sd).
#include <WString.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

class HalStorage {
 public:
  static HalStorage& getInstance() { static HalStorage s; return s; }
  std::string real(const char* p) const {
    const char* root = getenv("NEURON_SD");
    return std::string(root ? root : "sd") + p;
  }
  bool exists(const char* p) { return std::filesystem::exists(real(p)); }
  bool mkdir(const char* p, bool = true) { return std::filesystem::create_directories(real(p)) || exists(p); }
  bool readFileToString(const char*, const std::string& p, size_t cap, std::string& out) {
    std::ifstream f(real(p.c_str()), std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return !out.empty() && out.size() <= cap;
  }
  bool writeFile(const char* p, const String& content) {
    std::ofstream f(real(p), std::ios::binary | std::ios::trunc);
    f << content.str();
    return static_cast<bool>(f);
  }
};
#define Storage HalStorage::getInstance()
