/* Copyright 2017 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/backends/interpreter/platform.h"

#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "xla/backends/interpreter/executor.h"
#include "xla/stream_executor/device_options.h"
#include "xla/stream_executor/multi_platform_manager.h"
#include "xla/stream_executor/platform.h"
#include "xla/stream_executor/platform/initialize.h"
#include "tsl/platform/status.h"

namespace stream_executor {
namespace interpreter {

XlaInterpreterPlatform::XlaInterpreterPlatform(const std::string& name,
                                               const Platform::Id& id)
    : name_(name), id_(id) {}

XlaInterpreterPlatform::~XlaInterpreterPlatform() {}

Platform::Id XlaInterpreterPlatform::id() const { return id_; }

int XlaInterpreterPlatform::VisibleDeviceCount() const { return 1; }

const std::string& XlaInterpreterPlatform::Name() const { return name_; }

tsl::StatusOr<std::unique_ptr<DeviceDescription>>
XlaInterpreterPlatform::DescriptionForDevice(int ordinal) const {
  return XlaInterpreterExecutor::CreateDeviceDescription(ordinal);
}

tsl::StatusOr<StreamExecutor*> XlaInterpreterPlatform::ExecutorForDevice(
    int ordinal) {
  StreamExecutorConfig config;
  config.ordinal = ordinal;
  config.device_options = DeviceOptions::Default();
  return GetExecutor(config);
}

tsl::StatusOr<StreamExecutor*> XlaInterpreterPlatform::GetExecutor(
    const StreamExecutorConfig& config) {
  return executor_cache_.GetOrCreate(
      config, [&]() { return GetUncachedExecutor(config); });
}

tsl::StatusOr<std::unique_ptr<StreamExecutor>>
XlaInterpreterPlatform::GetUncachedExecutor(
    const StreamExecutorConfig& config) {
  auto executor = std::make_unique<StreamExecutor>(
      this, std::make_unique<XlaInterpreterExecutor>(), config.ordinal);
  auto init_status = executor->Init(config.device_options);
  if (!init_status.ok()) {
    return absl::Status{
        absl::StatusCode::kInternal,
        absl::StrFormat(
            "failed initializing StreamExecutor for device ordinal %d: %s",
            config.ordinal, init_status.ToString())};
  }

  return std::move(executor);
}

static void InitializeXlaInterpreterPlatform() {
  std::unique_ptr<Platform> platform(new XlaInterpreterPlatform);
  TF_CHECK_OK(MultiPlatformManager::RegisterPlatform(std::move(platform)));
}

}  // namespace interpreter
}  // namespace stream_executor

REGISTER_MODULE_INITIALIZER(
    interpreter_platform,
    stream_executor::interpreter::InitializeXlaInterpreterPlatform());
