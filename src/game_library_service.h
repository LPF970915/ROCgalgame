#pragma once

#include "config.h"
#include "game_library.h"

#include <filesystem>
#include <string>
#include <vector>

class GameLibraryService {
public:
  void Configure(const AppConfig &config);
  void LoadPersistentState();
  void Scan();

  const std::vector<GameEntry> &Games() const { return games_; }
  std::vector<int> VisibleIndices(int category_index) const;
  const GameEntry *GameAtVisibleIndex(const std::vector<int> &visible, int index) const;

  bool AddFavorite(const GameEntry &game);
  bool RemoveFavorite(const GameEntry &game);
  void PushHistory(const GameEntry &game);
  void ClearHistory();

  size_t FavoriteCount() const { return favorites_.size(); }
  size_t HistoryCount() const { return history_.size(); }

  static std::string NormalizePathKey(const std::filesystem::path &path);

private:
  std::filesystem::path CachePath(const char *name) const;
  static std::vector<std::string> LoadPathList(const std::filesystem::path &path);
  static void SavePathList(const std::filesystem::path &path,
                           const std::vector<std::string> &items);
  static bool Contains(const std::vector<std::string> &items, const std::string &key);

  std::filesystem::path root_;
  std::filesystem::path games_root_;
  std::filesystem::path covers_root_;
  std::filesystem::path saves_root_;
  std::vector<GameEntry> games_;
  std::vector<std::string> favorites_;
  std::vector<std::string> history_;
};
