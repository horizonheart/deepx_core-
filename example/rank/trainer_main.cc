// Copyright 2019 the deepx authors.
// Author: Yafei Zhang (kimmyzhang@tencent.com)
// Author: Shuting Guo (tinkleguo@tencent.com)
//

#include <deepx_core/common/any_map.h>
#include <deepx_core/common/stream.h>
#include <deepx_core/dx_log.h>
#include <deepx_core/graph/feature_kv_util.h>
#include <deepx_core/graph/graph.h>
#include <deepx_core/graph/model_shard.h>
#include <deepx_core/graph/shard.h>
#include <deepx_core/ps/file_dispatcher.h>
#include <deepx_core/tensor/data_type.h>
#include <gflags/gflags.h>
#include <cstdint>
#include <limits>  // std::numeric_limits
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "model_zoo.h"
#include "trainer_context.h"

// DEFINE_string(name, val, txt) 
DEFINE_string(instance_reader, "libsvm", "instance reader name");
DEFINE_string(instance_reader_config, "", "instance reader config");
DEFINE_string(model, "lr", "model name");
DEFINE_string(model_config, "", "model config");
DEFINE_string(optimizer, "adagrad", "optimizer name");
DEFINE_string(optimizer_config, "", "optimizer config");
DEFINE_int32(epoch, 1, "# of epochs");
DEFINE_int32(batch, 32, "batch size");
DEFINE_int32(thread, 1, "# of threads");
DEFINE_string(in, "", "input dir/file of training data");
DEFINE_int32(reverse_in, 0, "reverse input files");
DEFINE_int32(shuffle_in, 1, "shuffle input files for each epoch");
DEFINE_int32(model_shard, 0,
             "# of model shards, zero disables the model shard mode");
DEFINE_string(in_model, "", "input model dir");
DEFINE_string(warmup_model, "", "warmup model dir");
DEFINE_int32(out_model_remove_zeros, 0, "remove zeros from output model");
DEFINE_string(out_model, "", "output model dir(optional)");
DEFINE_string(out_text_model, "", "output text model dir(optional)");
DEFINE_string(out_feature_kv_model, "",
              "output feature kv model dir(optional)");
DEFINE_int32(out_feature_kv_protocol_version, 2,
             "output feature kv protocol version");
DEFINE_int32(verbose, 1, "verbose level: 0-10");
DEFINE_int32(seed, 9527, "seed of random engine");
DEFINE_int32(ts_enable, 0, "enable timestamp");
DEFINE_uint64(ts_now, 0, "timestamp of now");
DEFINE_uint64(ts_expire_threshold, 0, "timestamp expiration threshold");
DEFINE_uint64(freq_filter_threshold, 0,
              "feature frequency filtering threshold");

namespace deepx_core {
namespace {

Shard FLAGS_shard;

/************************************************************************/
/* Trainer */
/************************************************************************/
class Trainer {
 protected:
  Graph graph_;

  std::default_random_engine engine_;

  std::vector<std::string> files_;
  FileDispatcher file_dispatcher_;

  int epoch_ = 0;
  double epoch_loss_ = 0;
  double epoch_loss_weight_ = 0;
  std::mutex epoch_loss_mutex_;

  std::vector<std::unique_ptr<TrainerContext>> contexts_tls_;

 public:
  virtual ~Trainer() = default;
  virtual void Init();
  virtual void Train();
  void TrainEntry(int thread_id);
  virtual void TrainFile(int thread_id, const std::string& file);
  virtual void Save();
};

void Trainer::Init() {
  if (FLAGS_in_model.empty()) {
    std::unique_ptr<ModelZoo> model_zoo(NewModelZoo(FLAGS_model));
    DXCHECK_THROW(model_zoo);
    StringMap config;
    DXCHECK_THROW(ParseConfig(FLAGS_model_config, &config));
    DXCHECK_THROW(model_zoo->InitConfig(config));
    DXCHECK_THROW(model_zoo->InitGraph(&graph_));
  } else {
    DXCHECK_THROW(LoadGraph(FLAGS_in_model, &graph_));
  }
  DXINFO("Computational graph:\n%s", graph_.Dot().c_str());

  engine_.seed(FLAGS_seed);

  DXCHECK_THROW(AutoFileSystem::ListRecursive(FLAGS_in, true, &files_));

  file_dispatcher_.set_reverse(FLAGS_reverse_in);
  file_dispatcher_.set_shuffle(FLAGS_shuffle_in);
  file_dispatcher_.set_timeout(0);

  std::string new_path;
  if (AutoFileSystem::BackupIfExists(FLAGS_out_model, &new_path)) {
    DXINFO("Backed up %s to %s.", FLAGS_out_model.c_str(), new_path.c_str());
  }

  if (!AutoFileSystem::Exists(FLAGS_out_model)) {
    DXCHECK_THROW(AutoFileSystem::MakeDir(FLAGS_out_model));
  }

  if (!FLAGS_out_text_model.empty()) {
    if (AutoFileSystem::BackupIfExists(FLAGS_out_text_model, &new_path)) {
      DXINFO("Backed up %s to %s.", FLAGS_out_text_model.c_str(),
             new_path.c_str());
    }

    if (!AutoFileSystem::Exists(FLAGS_out_text_model)) {
      DXCHECK_THROW(AutoFileSystem::MakeDir(FLAGS_out_text_model));
    }
  }

  if (!FLAGS_out_feature_kv_model.empty()) {
    if (AutoFileSystem::BackupIfExists(FLAGS_out_feature_kv_model, &new_path)) {
      DXINFO("Backed up %s to %s.", FLAGS_out_feature_kv_model.c_str(),
             new_path.c_str());
    }

    if (!AutoFileSystem::Exists(FLAGS_out_feature_kv_model)) {
      DXCHECK_THROW(AutoFileSystem::MakeDir(FLAGS_out_feature_kv_model));
    }
  }
}

void Trainer::Train() {
  file_dispatcher_.PreTrain(files_);
  for (epoch_ = 0; epoch_ < FLAGS_epoch; ++epoch_) {
    DXINFO("Epoch %d begins.", epoch_ + 1);

    file_dispatcher_.PreEpoch();

    epoch_loss_ = 0;
    epoch_loss_weight_ = 0;

    std::vector<std::thread> threads;
    for (int j = 0; j < FLAGS_thread; ++j) {
      threads.emplace_back(&Trainer::TrainEntry, this, j);
    }
    for (std::thread& thread : threads) {
      thread.join();
    }

    DXINFO("Epoch %d completed.", epoch_ + 1);
  }
}

void Trainer::TrainEntry(int thread_id) {
  for (;;) {
    size_t file_size = 0;
    std::string file;
    if (!file_dispatcher_.WorkerDispatchFile(&file)) {
      DXINFO("[%d] [%3.1f%%] Training completed. ", thread_id, 100.0);
      break;
    }

    DXINFO("[%d] [%3.1f%%] Training %s...", thread_id,
           (100.0 - 100.0 * file_size / files_.size()), file.c_str());
    TrainFile(thread_id, file);
    (void)file_dispatcher_.WorkerFinishFile(file);
  }
}

void Trainer::TrainFile(int thread_id, const std::string& file) {
  TrainerContext* context = contexts_tls_[thread_id].get();
  context->TrainFile(thread_id, file);
  if (context->file_loss_weight() > 0) {
    double out_loss;
    {
      std::lock_guard<std::mutex> guard(epoch_loss_mutex_);
      epoch_loss_ += context->file_loss();
      epoch_loss_weight_ += context->file_loss_weight();
      out_loss = epoch_loss_ / epoch_loss_weight_;
    }
    DXINFO("epoch=%d, loss=%f", epoch_ + 1, out_loss);
  }
}

void Trainer::Save() {
  DXCHECK_THROW(SaveGraph(FLAGS_out_model, graph_));
  DXCHECK_THROW(SaveShard(FLAGS_out_model, FLAGS_shard));
}

/************************************************************************/
/* TrainerNonShard */
/************************************************************************/
class TrainerNonShard : public Trainer {
 private:
  ModelShard model_shard_;

 public:
  void Init() override;
  void Save() override;
};

void TrainerNonShard::Init() {
  Trainer::Init();

  model_shard_.seed(FLAGS_seed);
  model_shard_.InitShard(&FLAGS_shard, 0);
  model_shard_.InitGraph(&graph_);
  if (FLAGS_in_model.empty()) {
    DXCHECK_THROW(model_shard_.InitModel());
    DXCHECK_THROW(
        model_shard_.InitOptimizer(FLAGS_optimizer, FLAGS_optimizer_config));
  } else {
    DXCHECK_THROW(model_shard_.LoadModel(FLAGS_in_model));
    DXCHECK_THROW(
        model_shard_.LoadOptimizer(FLAGS_in_model, FLAGS_optimizer_config));
  }
  if (!FLAGS_warmup_model.empty()) {
    DXCHECK_THROW(model_shard_.WarmupModel(FLAGS_warmup_model));
    DXCHECK_THROW(model_shard_.WarmupOptimizer(FLAGS_warmup_model));
  }

  DXCHECK_THROW(!model_shard_.model().HasSRM());

  contexts_tls_.resize(FLAGS_thread);
  for (int i = 0; i < FLAGS_thread; ++i) {
    std::unique_ptr<TrainerContextNonShard> context(new TrainerContextNonShard);
    context->set_instance_reader(FLAGS_instance_reader);
    context->set_instance_reader_config(FLAGS_instance_reader_config);
    context->set_batch(FLAGS_batch);
    context->set_verbose(FLAGS_verbose);
    // Check out graph target conventions.
    context->set_target_name(graph_.target(0).name());
    context->Init(&model_shard_);
    contexts_tls_[i] = std::move(context);
  }
}

void TrainerNonShard::Save() {
  Trainer::Save();
  if (FLAGS_out_model_remove_zeros) {
    model_shard_.mutable_model()->RemoveZerosSRM();
  }
  DXCHECK_THROW(model_shard_.SaveModel(FLAGS_out_model));
  if (!FLAGS_out_text_model.empty()) {
    DXCHECK_THROW(model_shard_.SaveTextModel(FLAGS_out_text_model));
  }
  if (!FLAGS_out_feature_kv_model.empty()) {
    DXCHECK_THROW(model_shard_.SaveFeatureKVModel(
        FLAGS_out_feature_kv_model, FLAGS_out_feature_kv_protocol_version));
  }
  DXCHECK_THROW(model_shard_.SaveOptimizer(FLAGS_out_model));
}

/************************************************************************/
/* TrainerShard */
/************************************************************************/
class TrainerShard : public Trainer {
 private:
  int shard_size_ = 0;
  std::vector<ModelShard> model_shards_;
  std::vector<ModelShard> model_shards_tls_;

 public:
  void Init() override;
  void Train() override;
  void Save() override;
};

void TrainerShard::Init() {
  Trainer::Init();

  shard_size_ = FLAGS_shard.shard_size();
  model_shards_.resize(shard_size_);
  for (int i = 0; i < shard_size_; ++i) {
    model_shards_[i].seed(FLAGS_seed + i * 10099);  // magic number
    model_shards_[i].InitShard(&FLAGS_shard, i);
    model_shards_[i].InitGraph(&graph_);
    if (FLAGS_in_model.empty()) {
      DXCHECK_THROW(model_shards_[i].InitModel());
      DXCHECK_THROW(model_shards_[i].InitOptimizer(FLAGS_optimizer,
                                                   FLAGS_optimizer_config));
      if (FLAGS_ts_enable) {
        DXCHECK_THROW(model_shards_[i].InitTSStore(
            (DataType::ts_t)FLAGS_ts_now,
            (DataType::ts_t)FLAGS_ts_expire_threshold));
      }
      if (FLAGS_freq_filter_threshold > 0) {
        DXCHECK_THROW(model_shards_[i].InitFreqStore(
            (DataType::freq_t)FLAGS_freq_filter_threshold));
      }
    } else {
      DXCHECK_THROW(model_shards_[i].LoadModel(FLAGS_in_model));
      DXCHECK_THROW(model_shards_[i].LoadOptimizer(FLAGS_in_model,
                                                   FLAGS_optimizer_config));
      if (FLAGS_ts_enable) {
        if (!model_shards_[i].LoadTSStore(FLAGS_in_model, FLAGS_ts_now,
                                          FLAGS_ts_expire_threshold)) {
          DXCHECK_THROW(model_shards_[i].InitTSStore(
              (DataType::ts_t)FLAGS_ts_now,
              (DataType::ts_t)FLAGS_ts_expire_threshold));
        }
      }
      if (FLAGS_freq_filter_threshold > 0) {
        if (!model_shards_[i].LoadFreqStore(
                FLAGS_in_model,
                (DataType::freq_t)FLAGS_freq_filter_threshold)) {
          DXCHECK_THROW(model_shards_[i].InitFreqStore(
              (DataType::freq_t)FLAGS_freq_filter_threshold));
        }
      }
    }
    if (!FLAGS_warmup_model.empty()) {
      DXCHECK_THROW(model_shards_[i].WarmupModel(FLAGS_warmup_model));
      DXCHECK_THROW(model_shards_[i].WarmupOptimizer(FLAGS_warmup_model));
      if (FLAGS_ts_enable) {
        DXCHECK_THROW(model_shards_[i].WarmupTSStore(FLAGS_warmup_model));
      }
      if (FLAGS_freq_filter_threshold > 0) {
        DXCHECK_THROW(model_shards_[i].WarmupFreqStore(FLAGS_warmup_model));
      }
    }
  }

  for (int i = 0; i < shard_size_; ++i) {
    DXCHECK_THROW(model_shards_[i].model().HasSRM());
  }

  contexts_tls_.resize(FLAGS_thread);
  model_shards_tls_.resize(FLAGS_thread);
  for (int i = 0; i < FLAGS_thread; ++i) {
    model_shards_tls_[i].InitShard(&FLAGS_shard, 0);
    model_shards_tls_[i].InitGraph(&graph_);
    DXCHECK_THROW(model_shards_tls_[i].InitModelPlaceholder());
    std::unique_ptr<TrainerContextShard> context(new TrainerContextShard);
    context->set_instance_reader(FLAGS_instance_reader);
    context->set_instance_reader_config(FLAGS_instance_reader_config);
    context->set_batch(FLAGS_batch);
    context->set_verbose(FLAGS_verbose);
    if (FLAGS_freq_filter_threshold > 0) {
      context->set_freq_filter_threshold(
          (DataType::freq_t)FLAGS_freq_filter_threshold);
    }
    // Check out graph target conventions.
    context->set_target_name(graph_.target(0).name());
    context->Init(&model_shards_, &model_shards_tls_[i]);
    contexts_tls_[i] = std::move(context);
  }
}

void TrainerShard::Train() {
  for (int i = 0; i < shard_size_; ++i) {
    DXCHECK_THROW(model_shards_[i].InitThreadPool());
    model_shards_[i].StartThreadPool();
  }

  Trainer::Train();

  for (int i = 0; i < shard_size_; ++i) {
    model_shards_[i].StopThreadPool();
  }
}

void TrainerShard::Save() {
  Trainer::Save();
  for (int i = 0; i < shard_size_; ++i) {
    if (FLAGS_out_model_remove_zeros) {
      model_shards_[i].mutable_model()->RemoveZerosSRM();
    }
    if (FLAGS_ts_enable && FLAGS_ts_expire_threshold > 0) {
      model_shards_[i].ExpireTSStore();
    }
    DXCHECK_THROW(model_shards_[i].SaveModel(FLAGS_out_model));
    if (!FLAGS_out_text_model.empty()) {
      DXCHECK_THROW(model_shards_[i].SaveTextModel(FLAGS_out_text_model));
    }
    if (!FLAGS_out_feature_kv_model.empty()) {
      DXCHECK_THROW(model_shards_[i].SaveFeatureKVModel(
          FLAGS_out_feature_kv_model, FLAGS_out_feature_kv_protocol_version));
    }
    DXCHECK_THROW(model_shards_[i].SaveOptimizer(FLAGS_out_model));
    if (FLAGS_ts_enable) {
      DXCHECK_THROW(model_shards_[i].SaveTSStore(FLAGS_out_model));
    }
    if (FLAGS_freq_filter_threshold > 0) {
      DXCHECK_THROW(model_shards_[i].SaveFreqStore(FLAGS_out_model));
    }
  }
}

/************************************************************************/
/* main */
/************************************************************************/

// 配置文件检查
void CheckFlags() {
  AutoFileSystem fs;

  // DXCHECK_THROW 参数为false的时候，会跑出异常
  
  // 检查参数是否对得上
  DXCHECK_THROW(!FLAGS_instance_reader.empty());
  StringMap config;
  DXCHECK_THROW(ParseConfig(FLAGS_instance_reader_config, &config));
  if (config.count("batch") > 0) {
    int batch = std::stoi(config.at("batch"));
    if (batch != FLAGS_batch) {
      DXINFO(
          "Batch size from --instance_reader_config and --batch are "
          "inconsistent, use %d.",
          batch);
      FLAGS_batch = batch;
    }
  }
  DXCHECK_THROW(FLAGS_epoch > 0);
  DXCHECK_THROW(FLAGS_batch > 0);
  DXCHECK_THROW(FLAGS_thread > 0);

  //Canonicalize 规范化 规范化路径
  CanonicalizePath(&FLAGS_in);
  DXCHECK_THROW(!FLAGS_in.empty());
  DXCHECK_THROW(fs.Open(FLAGS_in));

  if (IsStdinStdoutPath(FLAGS_in)) {
    if (FLAGS_epoch != 1) {
      DXINFO("--epoch will be set to 1.");
      FLAGS_epoch = 1;
    }
    if (FLAGS_thread != 1) {
      DXINFO("--thread will be set to 1.");
      FLAGS_thread = 1;
    }
  }

  CanonicalizePath(&FLAGS_in_model);
  if (FLAGS_in_model.empty()) {
    DXCHECK_THROW(!FLAGS_model.empty());
    DXCHECK_THROW(!FLAGS_optimizer.empty());
    DXCHECK_THROW(FLAGS_model_shard >= 0);
  } else {
    DXCHECK_THROW(fs.Open(FLAGS_in_model));
    DXCHECK_THROW(!IsStdinStdoutPath(FLAGS_in_model));
  }
  CanonicalizePath(&FLAGS_warmup_model);
  if (!FLAGS_warmup_model.empty()) {
    DXCHECK_THROW(fs.Open(FLAGS_warmup_model));
    DXCHECK_THROW(!IsStdinStdoutPath(FLAGS_warmup_model));
  }

  CanonicalizePath(&FLAGS_out_model);
  if (FLAGS_out_model.empty()) {
    if (IsStdinStdoutPath(FLAGS_in)) {
      FLAGS_out_model = "stdin.train";
    } else {
      FLAGS_out_model = FLAGS_in + ".train";
    }
    DXINFO("Didn't specify --out_model, output to: %s.",
           FLAGS_out_model.c_str());
  }
  DXCHECK_THROW(fs.Open(FLAGS_out_model));
  DXCHECK_THROW(!IsStdinStdoutPath(FLAGS_out_model));

  CanonicalizePath(&FLAGS_out_text_model);
  if (!FLAGS_out_text_model.empty()) {
    DXCHECK_THROW(fs.Open(FLAGS_out_text_model));
    DXCHECK_THROW(!IsStdinStdoutPath(FLAGS_out_text_model));
  }

  CanonicalizePath(&FLAGS_out_feature_kv_model);
  if (!FLAGS_out_feature_kv_model.empty()) {
    DXCHECK_THROW(fs.Open(FLAGS_out_feature_kv_model));
    DXCHECK_THROW(!IsStdinStdoutPath(FLAGS_out_feature_kv_model));
    FeatureKVUtil::CheckVersion(FLAGS_out_feature_kv_protocol_version);
  }

  DXCHECK_THROW(FLAGS_verbose >= 0);

  if (FLAGS_ts_enable) {
    DXCHECK_THROW(FLAGS_ts_now <=
                  (google::uint64)std::numeric_limits<DataType::ts_t>::max());
    DXCHECK_THROW(FLAGS_ts_expire_threshold <=
                  (google::uint64)std::numeric_limits<DataType::ts_t>::max());
  }

  DXCHECK_THROW(FLAGS_freq_filter_threshold <=
                (google::uint64)std::numeric_limits<DataType::freq_t>::max());

  if (FLAGS_model_shard == 0) {
    FLAGS_shard.InitNonShard();
  } else {
    FLAGS_shard.InitShard(FLAGS_model_shard, "default");
  }
}

// todo main函数
int main(int argc, char** argv) {
  google::SetUsageMessage("Usage: [Options]");
#if HAVE_COMPILE_FLAGS_H == 1
  google::SetVersionString("\n\n"
#include "compile_flags.h"
  );
#endif
  // 命令行参数处理的开源库
  google::ParseCommandLineFlags(&argc, &argv, true);

  CheckFlags();

  std::unique_ptr<Trainer> trainer;
  if (FLAGS_shard.shard_mode() == 0) {
    trainer.reset(new TrainerNonShard);
  } else {
    trainer.reset(new TrainerShard);
  }
  trainer->Init();
  trainer->Train();
  trainer->Save();

  google::ShutDownCommandLineFlags();
  return 0;
}

}  // namespace
}  // namespace deepx_core

int main(int argc, char** argv) { return deepx_core::main(argc, argv); }
