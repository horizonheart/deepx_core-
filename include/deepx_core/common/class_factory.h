// Copyright 2019 the deepx authors.
// Author: Yafei Zhang (kimmyzhang@tencent.com)
//

#pragma once
#include <deepx_core/dx_log.h>
#include <set>
#include <string>
#include <type_traits>  // std::enable_if, ...
#include <unordered_map>

namespace deepx_core {

/************************************************************************/
/* ClassFactory 创建类的泛型工厂*/
/************************************************************************/
template <typename T>
class ClassFactory {
 public:
  using value_type = T;
  using pointer = value_type*;
  // 函数指针
  using creator = pointer (*)(); 

 private:
  std::unordered_map<std::string, creator> map_;
  std::set<std::string> set_;

 public:
  void Register(const std::string& name, creator creator) {
    if(name.compare("deepfm")==0){
      DXINFO("--thread will be set to 1.");
    }
    if (map_.count(name) > 0) {
      DXTHROW_INVALID_ARGUMENT("Duplicate registered name: %s.", name.c_str());
    }
    map_.emplace(name, creator);
    set_.emplace(name);
  }

  // 创建新的实例
  pointer New(const std::string& name) const {
    auto it = map_.find(name);
    if (it == map_.end()) {
      DXERROR("Unregistered name: %s.", name.c_str());
      return nullptr;
    }
    return it->second();
  }

  const std::set<std::string>& Names() const noexcept { return set_; }

 private:
//  私有构造函数，为了类成为单例
  ClassFactory() = default; 

 public:
  // 对外开放的接口，获取单例
  static ClassFactory& GetInstance() {
    static ClassFactory factory;
    return factory;
  }
};

/************************************************************************/
/* ClassFactoryRegister */
/************************************************************************/
template <typename T, typename U,
          typename =
              typename std::enable_if<std::is_default_constructible<U>::value &&
                                      std::is_convertible<U*, T*>::value>::type>
class ClassFactoryRegister {
 private:
  static T* Create() { return new U; }

 public:
  explicit ClassFactoryRegister(const std::string& name) {
    ClassFactory<T>::GetInstance().Register(name, &Create);
  }
};

/************************************************************************/
/* Helper macros */
/************************************************************************/
#define _CLASS_FACTORY_CONCAT_IMPL(x, y, z) _##x##_##y##_##z
#define _CLASS_FACTORY_CONCAT(x, y, z) _CLASS_FACTORY_CONCAT_IMPL(x, y, z)
#define CLASS_FACTORY_REGISTER(T, U, name)                      \
  deepx_core::ClassFactoryRegister<T, U> _CLASS_FACTORY_CONCAT( \
      T, U, __COUNTER__)(name)

// todo 定义类的动物园
#define CLASS_FACTORY_NEW(T, name) \
  deepx_core::ClassFactory<T>::GetInstance().New(name)

#define CLASS_FACTORY_NAMES(T) \
  deepx_core::ClassFactory<T>::GetInstance().Names()

}  // namespace deepx_core
