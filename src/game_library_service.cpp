#include "game_library_service.h"

#include "game_scanner.h"

#include <algorithm>
#include <fstream>

namespace {
std::string NativePathString(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}
}  // namespace

void GameLibraryService::Configure(const AppConfig &config) {
  root_ = config.root;
  games_root_ = config.games_root;
  covers_root_ = config.covers_root;
  saves_root_ = config.saves_root;
}

void GameLibraryService::LoadPersistentState() {
  favorites_ = LoadPathList(CachePath("favorites.txt"));
  history_ = LoadPathList(CachePath("history.txt"));
}

void GameLibraryService::Scan() {
  games_ = ScanGameLibrary(root_, games_root_, covers_root_, saves_root_);
}

std::vector<int> GameLibraryService::VisibleIndices(int category_index) const {
  std::vector<int> visible;
  visible.reserve(games_.size());
  for (int i = 0; i < static_cast<int>(games_.size()); ++i) {
    const GameEntry &game = games_[static_cast<size_t>(i)];
    const std::string key = NormalizePathKey(game.path);
    const bool matches = category_index == 0 ? game.core == CoreKind::Ons
                         : category_index == 1 ? game.core == CoreKind::Krkr
                         : category_index == 2 ? Contains(favorites_, key)
                                               : Contains(history_, key);
    if (matches) visible.push_back(i);
  }
  if (category_index == 3) {
    std::stable_sort(visible.begin(), visible.end(), [&](int left, int right) {
      const std::string left_key = NormalizePathKey(games_[static_cast<size_t>(left)].path);
      const std::string right_key = NormalizePathKey(games_[static_cast<size_t>(right)].path);
      const auto left_it = std::find(history_.begin(), history_.end(), left_key);
      const auto right_it = std::find(history_.begin(), history_.end(), right_key);
      return left_it < right_it;
    });
  }
  return visible;
}

const GameEntry *GameLibraryService::GameAtVisibleIndex(
    const std::vector<int> &visible, int index) const {
  if (visible.empty()) return nullptr;
  index = std::clamp(index, 0, static_cast<int>(visible.size()) - 1);
  const int game_index = visible[static_cast<size_t>(index)];
  if (game_index < 0 || game_index >= static_cast<int>(games_.size())) return nullptr;
  return &games_[static_cast<size_t>(game_index)];
}

bool GameLibraryService::AddFavorite(const GameEntry &game) {
  const std::string key = NormalizePathKey(game.path);
  if (key.empty() || Contains(favorites_, key)) return false;
  favorites_.push_back(key);
  SavePathList(CachePath("favorites.txt"), favorites_);
  return true;
}

bool GameLibraryService::RemoveFavorite(const GameEntry &game) {
  const std::string key = NormalizePathKey(game.path);
  auto found = std::find(favorites_.begin(), favorites_.end(), key);
  if (found == favorites_.end()) return false;
  favorites_.erase(found);
  SavePathList(CachePath("favorites.txt"), favorites_);
  return true;
}

void GameLibraryService::PushHistory(const GameEntry &game) {
  const std::string key = NormalizePathKey(game.path);
  if (key.empty()) return;
  history_.erase(std::remove(history_.begin(), history_.end(), key), history_.end());
  history_.insert(history_.begin(), key);
  if (history_.size() > 64) history_.resize(64);
  SavePathList(CachePath("history.txt"), history_);
}

void GameLibraryService::ClearHistory() {
  history_.clear();
  SavePathList(CachePath("history.txt"), history_);
}

std::string GameLibraryService::NormalizePathKey(const std::filesystem::path &path) {
  try {
    return ToLowerAscii(NativePathString(std::filesystem::weakly_canonical(path)));
  } catch (...) {
    try {
      return ToLowerAscii(NativePathString(path));
    } catch (...) {
      return {};
    }
  }
}

std::filesystem::path GameLibraryService::CachePath(const char *name) const {
  return root_ / "cache" / name;
}

std::vector<std::string> GameLibraryService::LoadPathList(
    const std::filesystem::path &path) {
  std::vector<std::string> items;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (!line.empty()) items.push_back(line);
  }
  return items;
}

void GameLibraryService::SavePathList(const std::filesystem::path &path,
                                      const std::vector<std::string> &items) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out) return;
  for (const std::string &item : items) out << item << '\n';
}

bool GameLibraryService::Contains(const std::vector<std::string> &items,
                                  const std::string &key) {
  return std::find(items.begin(), items.end(), key) != items.end();
}
