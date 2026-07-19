#include "version_update_runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>
#include <vector>

namespace {
constexpr const char *kPendingMarker = "ROCgalgame_update_pending.txt";
constexpr const char *kManifestTemp = "ROCgalgame_update_manifest.tmp";
constexpr const char *kDownloadTemp = "ROCgalgame_update_download.tmp";

std::string TrimCopy(std::string value) {
  const auto space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && space(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && space(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}

std::string ReadFirstLine(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::string line;
  if (in) std::getline(in, line);
  return TrimCopy(line);
}

std::string ShellQuote(const std::string &value) {
#ifdef _WIN32
  std::string out = "\"";
  for (char ch : value) out += ch == '\"' ? "\\\"" : std::string(1, ch);
  return out + "\"";
#else
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') out += "'\\''";
    else out.push_back(ch);
  }
  return out + "'";
#endif
}

bool DownloadToFile(const std::string &url, const std::filesystem::path &path) {
  if (url.empty() || path.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
#ifdef _WIN32
  const std::string command =
      "curl.exe -L --fail --silent --show-error --connect-timeout 15 --max-time 300 "
      "-A ROCgalgame-Updater -o " + ShellQuote(path.string()) + " " + ShellQuote(url);
  return std::system(command.c_str()) == 0;
#else
  const std::string clean_env = "env -u LD_LIBRARY_PATH ";
  const std::string curl = clean_env +
      "curl -L --fail --silent --show-error --connect-timeout 15 --max-time 300 "
      "-A ROCgalgame-Updater -o " + ShellQuote(path.string()) + " " + ShellQuote(url);
  if (std::system(curl.c_str()) == 0) return true;
  const std::string wget = clean_env +
      "wget -q --timeout=300 --user-agent=ROCgalgame-Updater -O " +
      ShellQuote(path.string()) + " " + ShellQuote(url);
  return std::system(wget.c_str()) == 0;
#endif
}

int VersionNumber(const std::string &version) {
  std::smatch match;
  if (!std::regex_search(version, match, std::regex(R"(([0-9]+)\.([0-9]{2}))"))) return -1;
  try {
    return std::stoi(match[1].str()) * 100 + std::stoi(match[2].str());
  } catch (...) {
    return -1;
  }
}

struct PackageCandidate {
  std::string filename;
  std::string version;
  std::string download_url;
  uint64_t size = 0;
};

std::vector<PackageCandidate> ParsePackageCandidates(const std::string &json) {
  const std::regex package_pattern(
      R"REGEX("name"\s*:\s*"(ROCgalgame ver([0-9]+\.[0-9]{2}) for GKD350H Ultra\.zip)"[\s\S]*?"size"\s*:\s*([0-9]+)[\s\S]*?"download_url"\s*:\s*"([^"]+)")REGEX");
  std::vector<PackageCandidate> out;
  for (std::sregex_iterator it(json.begin(), json.end(), package_pattern), end; it != end; ++it) {
    PackageCandidate candidate;
    candidate.filename = (*it)[1].str();
    candidate.version = (*it)[2].str();
    candidate.download_url = (*it)[4].str();
    try { candidate.size = std::stoull((*it)[3].str()); } catch (...) {}
    out.push_back(std::move(candidate));
  }
  return out;
}

VersionUpdateThreadResult RunUpdate(const std::filesystem::path &runtime_root,
                                    const std::filesystem::path &downloads_root,
                                    const std::string &manifest_url,
                                    const std::string &current_version,
                                    std::atomic<uint64_t> &expected_bytes) {
  VersionUpdateThreadResult result;
  const std::filesystem::path manifest_path = downloads_root / kManifestTemp;
  std::error_code ec;
  std::filesystem::remove(manifest_path, ec);
  if (!DownloadToFile(manifest_url, manifest_path)) {
    result.outcome = VersionUpdateThreadResult::Outcome::NoNetwork;
    return result;
  }

  std::ifstream manifest_file(manifest_path, std::ios::binary);
  std::ostringstream manifest_buffer;
  manifest_buffer << manifest_file.rdbuf();
  std::filesystem::remove(manifest_path, ec);
  const std::vector<PackageCandidate> candidates = ParsePackageCandidates(manifest_buffer.str());
  if (candidates.empty()) return result;

  const PackageCandidate *latest = nullptr;
  for (const PackageCandidate &candidate : candidates) {
    if (!latest || VersionNumber(candidate.version) > VersionNumber(latest->version)) latest = &candidate;
  }
  if (!latest) return result;
  result.latest_version = latest->version;
  result.filename = latest->filename;
  result.expected_bytes = latest->size;
  expected_bytes.store(latest->size, std::memory_order_release);
  if (VersionNumber(latest->version) <= VersionNumber(current_version)) {
    result.outcome = VersionUpdateThreadResult::Outcome::UpToDate;
    return result;
  }

  const std::filesystem::path temp_path = downloads_root / kDownloadTemp;
  const std::filesystem::path package_path = downloads_root / latest->filename;
  std::filesystem::remove(temp_path, ec);
  if (!DownloadToFile(latest->download_url, temp_path)) {
    std::filesystem::remove(temp_path, ec);
    return result;
  }
  if (latest->size > 0 && std::filesystem::file_size(temp_path, ec) != latest->size) {
    std::filesystem::remove(temp_path, ec);
    return result;
  }
  std::filesystem::remove(package_path, ec);
  std::filesystem::rename(temp_path, package_path, ec);
  if (ec) return result;

  std::ofstream marker(downloads_root / kPendingMarker, std::ios::trunc);
  marker << "filename=" << latest->filename << "\n";
  marker << "version=" << latest->version << "\n";
  marker.flush();
  if (!marker) return result;
  result.outcome = VersionUpdateThreadResult::Outcome::Downloaded;
  (void)runtime_root;
  return result;
}
}  // namespace

bool HandleVersionUpdateInput(const InputManager &input, VersionUpdateState &state,
                              const VersionUpdateCallbacks &callbacks) {
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    if (callbacks.close_panel) callbacks.close_panel();
    return true;
  }
  if (input.IsJustPressed(Button::A) && callbacks.start_check) callbacks.start_check(state);
  return true;
}

void InitializeVersionUpdateState(VersionUpdateState &state,
                                  const std::filesystem::path &runtime_root,
                                  const std::string &manifest_url) {
  ShutdownVersionUpdateState(state);
  state = VersionUpdateState{};
  state.runtime_root = runtime_root;
  state.downloads_root = runtime_root / "Downloads";
  state.temp_package_path = state.downloads_root / kDownloadTemp;
  state.manifest_url = manifest_url;
  std::error_code ec;
  std::filesystem::create_directories(state.downloads_root, ec);
  const std::string installed = ReadFirstLine(runtime_root / "version.txt");
  if (!installed.empty()) state.current_version = installed;
  if (state.manifest_url.empty()) state.status = VersionUpdateStatus::Unconfigured;
}

bool BeginVersionUpdateDownload(VersionUpdateState &state) {
  if (state.manifest_url.empty()) {
    state.status = VersionUpdateStatus::Unconfigured;
    return false;
  }
  if (state.worker.joinable() || state.status == VersionUpdateStatus::Downloaded) return false;
  state.status = VersionUpdateStatus::Downloading;
  state.download_progress_pct = 0;
  state.latest_version.clear();
  state.expected_download_bytes = 0;
  state.worker_state = std::make_shared<VersionUpdateThreadState>();
  const auto shared = state.worker_state;
  const auto root = state.runtime_root;
  const auto downloads = state.downloads_root;
  const auto url = state.manifest_url;
  const auto current = state.current_version;
  state.worker = std::thread([shared, root, downloads, url, current]() {
    shared->result = RunUpdate(root, downloads, url, current, shared->expected_bytes);
    shared->done.store(true, std::memory_order_release);
  });
  return true;
}

bool TickVersionUpdateState(VersionUpdateState &state) {
  bool changed = false;
  if (state.worker_state && state.expected_download_bytes == 0) {
    state.expected_download_bytes =
        state.worker_state->expected_bytes.load(std::memory_order_acquire);
  }
  if (state.status == VersionUpdateStatus::Downloading && state.expected_download_bytes > 0) {
    std::error_code ec;
    const uint64_t bytes = std::filesystem::exists(state.temp_package_path, ec)
                               ? std::filesystem::file_size(state.temp_package_path, ec)
                               : 0;
    const int progress = static_cast<int>(std::min<uint64_t>(99, bytes * 100 / state.expected_download_bytes));
    if (progress != state.download_progress_pct) {
      state.download_progress_pct = progress;
      changed = true;
    }
  }
  if (!state.worker_state || !state.worker_state->done.load(std::memory_order_acquire)) return changed;
  if (state.worker.joinable()) state.worker.join();
  const VersionUpdateThreadResult result = state.worker_state->result;
  state.worker_state.reset();
  state.latest_version = result.latest_version;
  state.expected_download_bytes = result.expected_bytes;
  switch (result.outcome) {
    case VersionUpdateThreadResult::Outcome::NoNetwork:
      state.status = VersionUpdateStatus::NoNetwork;
      break;
    case VersionUpdateThreadResult::Outcome::UpToDate:
      state.status = VersionUpdateStatus::UpToDate;
      break;
    case VersionUpdateThreadResult::Outcome::Downloaded:
      state.status = VersionUpdateStatus::Downloaded;
      state.download_progress_pct = 100;
      break;
    default:
      state.status = VersionUpdateStatus::DownloadFailed;
      break;
  }
  return true;
}

void ShutdownVersionUpdateState(VersionUpdateState &state) {
  if (state.worker.joinable()) state.worker.join();
  state.worker_state.reset();
}
