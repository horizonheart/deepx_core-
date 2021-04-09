// Copyright 2019 the deepx authors.
// Author: Yafei Zhang (kimmyzhang@tencent.com)
//

#pragma once
#include <sstream>
#include <string>
#include <vector>

namespace deepx_core {

// Trim leading and trailing spaces, tabs and line endings.
std::string& Trim(std::string* s) noexcept;

// Return if 's' begins with 'beginning'.
bool BeginWith(const std::string& s, const std::string& beginning) noexcept;

// Return if 's' ends with 'ending'.
bool EndWith(const std::string& s, const std::string& ending) noexcept;

// Split 's' with 'sep'.
// If 'discard_empty' is true, empty string will be discarded.
void Split(const std::string& s, const std::string& sep,
           std::vector<std::string>* vs, bool discard_empty = true);

// 利用分隔符sep切割字符串s
// If 'discard_empty' is true, empty string will be discarded.
//
// Return if all std::string objects are converted to T objects.
template <typename T>
bool Split(const std::string& s, const std::string& sep, std::vector<T>* vt,
           bool discard_empty = true) {
  std::vector<std::string> vs;
  Split(s, sep, &vs, discard_empty);
  vt->resize(vs.size()); //将切割后的字符串存放到vt中
  for (size_t i = 0; i < vs.size(); ++i) {
    // istringstream::istringstream(string str);
    // 它的作用是从string对象str中读取字符。
    std::istringstream is(vs[i]);
    is >> (*vt)[i];
    if (!is || !is.eof()) {
      vt->clear();
      return false;
    }
  }
  return true;
}

// Join 'vs' with 'sep'.
std::string Join(const std::vector<std::string>& vs, const std::string& sep);

}  // namespace deepx_core
