#pragma once

#include "input_manager.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

enum class VersionUpdateStatus {
  Idle,
  Unconfigured,
  NoNetwork,
  Downloading,
  Downloaded,
  UpToDate,
  DownloadFailed,
};

struct VersionUpdateThreadResult {
  enum class Outcome { Failed, NoNetwork, UpToDate, Downloaded };
  Outcome outcome = Outcome::Failed;
  std::string latest_version;
  std::string filename;
  uint64_t expected_bytes = 0;
};

struct VersionUpdateThreadState {
  std::atomic<bool> done = false;
  std::atomic<uint64_t> expected_bytes = 0;
  VersionUpdateThreadResult result;
};

struct VersionUpdateState {
  VersionUpdateStatus status = VersionUpdateStatus::Idle;
  std::string current_version = "0.01";
  std::string latest_version;
  std::string manifest_url;
  std::filesystem::path runtime_root;
  std::filesystem::path downloads_root;
  std::filesystem::path temp_package_path;
  uint64_t expected_download_bytes = 0;
  int download_progress_pct = 0;
  std::thread worker;
  std::shared_ptr<VersionUpdateThreadState> worker_state;
};

struct VersionUpdateCallbacks {
  std::function<void()> close_panel;
  std::function<void(VersionUpdateState &)> start_check;
};

bool HandleVersionUpdateInput(const InputManager &input, VersionUpdateState &state,
                              const VersionUpdateCallbacks &callbacks);
void InitializeVersionUpdateState(VersionUpdateState &state,
                                  const std::filesystem::path &runtime_root,
                                  const std::string &manifest_url);
bool BeginVersionUpdateDownload(VersionUpdateState &state);
bool TickVersionUpdateState(VersionUpdateState &state);
void ShutdownVersionUpdateState(VersionUpdateState &state);
