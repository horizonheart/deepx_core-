// Copyright 2019 the deepx authors.
// Author: Yafei Zhang (kimmyzhang@tencent.com)
//

#include "model_zoo_impl.h"

namespace deepx_core {

/************************************************************************/
/* ModelZooImpl */
/************************************************************************/
bool ModelZooImpl::InitConfig(const AnyMap& config) {
  if (!PreInitConfig()) {
    return false;
  }

  for (const auto& entry : config) {
    const std::string& k = entry.first;
    const auto& v = entry.second.to_ref<std::string>();
    if (!InitConfigKV(k, v)) {
      return false;
    }
  }

  if (!PostInitConfig()) {
    return false;
  }
  return true;
}

// TODO 初始化配置
bool ModelZooImpl::InitConfig(const StringMap& config) {
  if (!PreInitConfig()) {
    return false;
  }

  for (const auto& entry : config) {
    const std::string& k = entry.first;
    const std::string& v = entry.second;
    // 初始化key value类型的配置
    if (!InitConfigKV(k, v)) {
      return false;
    }
  }

  if (!PostInitConfig()) {
    return false;
  }
  return true;
}

// todo 父类初始化key value类型的配置
bool ModelZooImpl::InitConfigKV(const std::string& k, const std::string& v) {
  if (k == "config" || k == "group_config") {
    // 加载配置文件的内容到items_
    if (!GuessGroupConfig(v, &items_, nullptr, k.c_str())) {
      return false;
    }
    // 判断要embedding的维度大小是否一样
    item_is_fm_ = IsFMGroupConfig(items_) ? 1 : 0;

    item_m_ = (int)items_.size();
    if (item_is_fm_) {
      item_k_ = items_.front().embedding_col;
    } else {
      item_k_ = 0;
    }
    // 总的embedding的大小
    item_mk_ = GetTotalEmbeddingCol(items_);
  } else if (k == "w" || k == "has_w") {
    has_w_ = std::stoi(v);
  } else if (k == "sparse") {
    sparse_ = std::stoi(v);
  } else {
    return false;
  }
  return true;
}

/************************************************************************/
/* ModelZoo functions 构造模型的动物园*/
/************************************************************************/
std::unique_ptr<ModelZoo> NewModelZoo(const std::string& name) {
  std::unique_ptr<ModelZoo> model_zoo(MODEL_ZOO_NEW(name));
  if (!model_zoo) {
    DXERROR("Invalid model name: %s.", name.c_str());
    DXERROR("Model name can be:");
    for (const std::string& _name : MODEL_ZOO_NAMES()) {
      DXERROR("  %s", _name.c_str());
    }
  }
  return model_zoo;
}

}  // namespace deepx_core
