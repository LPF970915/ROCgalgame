#include "cover_resolver.h"

#include "config.h"

namespace {
bool Exists(const std::filesystem::path &path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

bool IsImageExtension(const std::filesystem::path &path) {
  const std::string ext = ToLowerAscii(path.extension().u8string());
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
         ext == ".webp" || ext == ".bmp";
}

int CoverNameScore(const std::filesystem::path &path, int depth) {
  const std::string stem = ToLowerAscii(path.stem().u8string());
  const std::string filename = ToLowerAscii(path.filename().u8string());
  int score = 1000 - depth * 30;
  auto exact = [&](const char *value, int bonus) { if (stem == value) score += bonus; };
  auto contains = [&](const char *value, int bonus) {
    if (filename.find(value) != std::string::npos) score += bonus;
  };
  exact("cover", 800); exact("folder", 720); exact("poster", 700);
  exact("package", 680); exact("jacket", 660); exact("title", 620);
  exact("thumbnail", 580); exact("thumb", 560); exact("icon", 420);
  contains("cover", 520); contains("folder", 450); contains("poster", 430);
  contains("package", 410); contains("jacket", 390); contains("title", 360);
  contains("thumbnail", 330); contains("thumb", 300); contains("screenshot", 220);
  contains("screen", 180); contains("cg", 90); contains("logo", -120);
  contains("button", -260); contains("cursor", -260); contains("mask", -260);
  contains("sprite", -300);
  return score;
}

std::filesystem::path BestCoverInsideGameFolder(
    const std::filesystem::path &directory) {
  std::filesystem::path best;
  int best_score = -100000;
  int visited = 0;
  std::error_code ec;
  std::filesystem::recursive_directory_iterator it(
      directory, std::filesystem::directory_options::skip_permission_denied, ec);
  std::filesystem::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    if (it.depth() > 2) {
      it.disable_recursion_pending();
      continue;
    }
    if (++visited > 500) break;
    if (!it->is_regular_file(ec) || !IsImageExtension(it->path())) continue;
    const int score = CoverNameScore(it->path(), it.depth());
    if (best.empty() || score > best_score) {
      best = it->path();
      best_score = score;
    }
  }
  return best;
}
}  // namespace

std::filesystem::path ResolveGameCoverPath(
    const std::filesystem::path &covers_root,
    const std::filesystem::path &alternate_covers_root,
    const std::filesystem::path &game_directory) {
  const std::string base = game_directory.filename().u8string();
  for (const auto &root : {covers_root, alternate_covers_root}) {
    if (root.empty()) continue;
    for (const auto &extension : {".png", ".jpg", ".jpeg", ".webp", ".bmp"}) {
      const std::filesystem::path candidate = root / (base + extension);
      if (Exists(candidate)) return candidate;
    }
  }
  for (const auto &name : {
           "cover.png", "cover.jpg", "cover.jpeg", "cover.webp", "cover.bmp",
           "folder.png", "folder.jpg", "folder.jpeg", "folder.webp",
           "poster.png", "poster.jpg", "poster.jpeg", "poster.webp",
           "package.png", "package.jpg", "package.jpeg", "package.webp",
           "title.png", "title.jpg", "title.jpeg", "title.webp",
           "thumbnail.png", "thumbnail.jpg", "thumb.png", "thumb.jpg",
           "icon.png", "icon.jpg"}) {
    const std::filesystem::path candidate = game_directory / name;
    if (Exists(candidate)) return candidate;
  }
  return BestCoverInsideGameFolder(game_directory);
}
