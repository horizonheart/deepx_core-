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
  // 函数指针,返回类型为指针，参数为空
  using creator = pointer (*)(); 

 private:
  std::unordered_map<std::string, creator> map_;
  std::set<std::string> set_;

 public:
  // todo 进行工厂注册
  void Register(const std::string& name, creator creator) {
    // if(name.compare("deepfm")==0){
    //   DXINFO("--thread will be set to 1.");
    // }
    if (map_.count(name) > 0) {
      DXTHROW_INVALID_ARGUMENT("Duplicate registered name: %s.", name.c_str());
    }
    map_.emplace(name, creator);
    set_.emplace(name);
  }

  // 创建新的实例，根据已经名字，从已经注册的类中选择
  pointer New(const std::string& name) const {
    auto it = map_.find(name);
    if (it == map_.end()) {
      DXERROR("Unregistered name: %s.", name.c_str());
      return nullptr;
    }
    return it->second(); //获取value值，并调用对应的函数，创建对象
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
/* ClassFactoryRegister  工厂类的构造函数，使用*/
/************************************************************************/
// enable_if：满足条件时类型有效
// is_default_constructible: 测试类型是否具有默认构造函数
// is_convertible: 判断是否可以被转化  第一个参数的类型 可以转化为第二个参数的类型
// typename的作用就是告诉c++编译器，typename后面的字符串为一个类型名称，而不是成员函数或者成员变量
template <typename T, typename U,
          typename =
              typename std::enable_if<std::is_default_constructible<U>::value &&
                                      std::is_convertible<U*, T*>::value>::type>
class ClassFactoryRegister {
 private:
  // 调用无参的构造函数创建对象
  static T* Create() { return new U; }

 public:
//构造函数
  explicit ClassFactoryRegister(const std::string& name) {
    ClassFactory<T>::GetInstance().Register(name, &Create);
  }
};

/************************************************************************/
/* Helper macros */
/************************************************************************/

// 这些是自定义对象的名字
#define _CLASS_FACTORY_CONCAT_IMPL(x, y, z) _##x##_##y##_##z
#define _CLASS_FACTORY_CONCAT(x, y, z) _CLASS_FACTORY_CONCAT_IMPL(x, y, z)

// todo 比较重要的一个工厂类函数接口，根据泛型进行工厂模式的对象创建
// todo T是基类，U是T的子类，name是名字
// 宏__COUNTER__实质上是一个int，并且是具体的数，初值是0，每预编译一次其值自己加1
#define CLASS_FACTORY_REGISTER(T, U, name)                      \
  deepx_core::ClassFactoryRegister<T, U> _CLASS_FACTORY_CONCAT( \
      T, U, __COUNTER__)(name)

// todo 创建新的对象
#define CLASS_FACTORY_NEW(T, name) \
  deepx_core::ClassFactory<T>::GetInstance().New(name)

// todo 获取所有已经注册的类
#define CLASS_FACTORY_NAMES(T) \
  deepx_core::ClassFactory<T>::GetInstance().Names()

}  // namespace deepx_core
