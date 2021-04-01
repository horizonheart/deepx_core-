// Copyright 2019 the deepx authors.
// Author: Yafei Zhang (kimmyzhang@tencent.com)
//

#include <deepx_core/common/str_util.h>
#include <algorithm>  // std::equal

namespace deepx_core {

std::string& Trim(std::string* s) noexcept {
  size_t left = s->find_first_not_of(" \n\r\t");
  size_t right = s->find_last_not_of(" \n\r\t");
  if (right == std::string::npos && left == std::string::npos) {
    s->clear();
  } else {
    if (right != std::string::npos) {
      s->erase(right + 1);
    }
    if (left != std::string::npos) {
      s->erase(0, left);
    }
  }
  return *s;
}

bool BeginWith(const std::string& s, const std::string& beginning) noexcept {
  return s.find(beginning) == 0;
}

bool EndWith(const std::string& s, const std::string& ending) noexcept {
  if (ending.size() > s.size()) {
    return false;
  }
  return std::equal(ending.rbegin(), ending.rend(), s.rbegin());
}

// 字符串切割
void Split(const std::string& s, const std::string& sep,
           std::vector<std::string>* vs, bool discard_empty) {
  vs->clear();

  if (sep.empty()) {
    return;
  }

  // 当前要分割的字符串中，一个sep都没有
  size_t index = s.find(sep, 0);
  if (index == std::string::npos) {
    if (!s.empty()) {
      vs->emplace_back(s);
    }
    return;
  }

  // 循环遍历查找sep的位置
  size_t first = 0;
  while (index != std::string::npos) {
    if (!discard_empty || index > first) {
      vs->emplace_back(s.substr(first, index - first));
    }
    first = index + sep.size();
    index = s.find(sep, first);
  }

  //添加最后的部分
  if (!discard_empty || s.size() > first) {
    vs->emplace_back(s.substr(first));
  }
}

std::string Join(const std::vector<std::string>& vs, const std::string& sep) {
  if (vs.empty()) {
    return "";
  }

  size_t size = (vs.size() - 1) * sep.size();
  for (size_t i = 0; i < vs.size(); ++i) {  // NOLINT
    size += vs[i].size();
  }

  std::string s;
  s.reserve(size);
  s += vs[0];
  for (size_t i = 1; i < vs.size(); ++i) {
    s += sep;
    s += vs[i];
  }
  return s;
}

}  // namespace deepx_core
