#pragma once
#include <string>
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String& operator+=(const char* s) { s_ += s; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  bool reserve(size_t n) { s_.reserve(n); return true; }
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  const std::string& str() const { return s_; }
 private:
  std::string s_;
};
