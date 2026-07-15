#include "game_library.h"

#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
std::string NativePathString(const fs::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}

bool Exists(const fs::path &path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

bool IsDirectory(const fs::path &path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

std::string DisplayName(const fs::path &path) {
  try {
    return NativePathString(path.filename());
  } catch (...) {
    return "Untitled";
  }
}

bool HasAny(const fs::path &dir, const std::vector<std::string> &names) {
  for (const auto &name : names) {
    if (Exists(dir / name)) return true;
  }
  return false;
}

bool HasAnyExtension(const fs::path &dir, const std::vector<std::string> &extensions) {
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) continue;
    const std::string ext = ToLowerAscii(entry.path().extension().u8string());
    if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) return true;
  }
  return false;
}

CoreKind DetectCore(const fs::path &dir) {
  if (HasAny(dir, {"0.txt", "00.txt", "nscript.dat", "nscript.___", "arc.nsa", "arc.sar"})) return CoreKind::Ons;
  if (HasAny(dir, {"startup.tjs", "Config.tjs", "config.tjs", "data.xp3"}) ||
      HasAnyExtension(dir, {".xp3"})) {
    return CoreKind::Krkr;
  }
  return CoreKind::Unknown;
}

CoreKind ParseCoreKind(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  if (v == "ons" || v == "onscripter" || v == "onsyuri") return CoreKind::Ons;
  if (v == "krkr" || v == "kirikiri") return CoreKind::Krkr;
  if (v == "tyrano") return CoreKind::Tyrano;
  return CoreKind::Unknown;
}

bool IsAspectValue(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  return v == "stretch" || v == "contain" || v == "fit-width" || v == "fill-height";
}

bool IsFilterValue(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  return v == "clean" || v == "scanline" || v == "crt-soft" || v == "mask";
}

void ReadGameIni(const fs::path &dir, GameEntry &game) {
  std::ifstream in(dir / "game.ini");
  if (!in) return;
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = ToLowerAscii(Trim(line.substr(0, eq)));
    const std::string value = Trim(line.substr(eq + 1));
    if (key == "title" && !value.empty()) {
      game.title = value;
    } else if (key == "core") {
      CoreKind parsed = ParseCoreKind(value);
      if (parsed != CoreKind::Unknown) game.core = parsed;
    } else if (key == "encoding") {
      game.overrides.encoding = ToLowerAscii(value);
    } else if (key == "aspect" && IsAspectValue(value)) {
      game.overrides.aspect = ToLowerAscii(value);
    } else if (key == "filter" && IsFilterValue(value)) {
      game.overrides.filter = ToLowerAscii(value);
    } else if (key == "virtual_mouse") {
      game.overrides.has_virtual_mouse = true;
      game.overrides.virtual_mouse = IsTruthy(value);
    } else if (key == "mouse_speed") {
      try { game.overrides.mouse_speed = std::max(1, std::stoi(value)); } catch (...) {}
    } else if (key == "mouse_accel") {
      try { game.overrides.mouse_accel = std::max(0.1f, std::stof(value)); } catch (...) {}
    }
  }
}

bool IsImageExtension(const fs::path &path) {
  const std::string ext = ToLowerAscii(path.extension().u8string());
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp";
}

int CoverNameScore(const fs::path &path, int depth) {
  const std::string stem = ToLowerAscii(path.stem().u8string());
  const std::string filename = ToLowerAscii(path.filename().u8string());
  int score = 1000 - depth * 30;
  auto exact = [&](const char *value, int bonus) {
    if (stem == value) score += bonus;
  };
  auto contains = [&](const char *value, int bonus) {
    if (filename.find(value) != std::string::npos) score += bonus;
  };

  exact("cover", 800);
  exact("folder", 720);
  exact("poster", 700);
  exact("package", 680);
  exact("jacket", 660);
  exact("title", 620);
  exact("thumbnail", 580);
  exact("thumb", 560);
  exact("icon", 420);
  contains("cover", 520);
  contains("folder", 450);
  contains("poster", 430);
  contains("package", 410);
  contains("jacket", 390);
  contains("title", 360);
  contains("thumbnail", 330);
  contains("thumb", 300);
  contains("screenshot", 220);
  contains("screen", 180);
  contains("cg", 90);
  contains("logo", -120);
  contains("button", -260);
  contains("cursor", -260);
  contains("mask", -260);
  contains("sprite", -300);
  return score;
}

fs::path BestCoverInsideGameFolder(const fs::path &dir) {
  fs::path best;
  int best_score = -100000;
  int visited = 0;
  std::error_code ec;
  fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
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

fs::path CoverFor(const fs::path &covers_root, const fs::path &alternate_covers_root, const fs::path &dir) {
  const std::string base = DisplayName(dir);
  for (const auto &root : {covers_root, alternate_covers_root}) {
    if (root.empty()) continue;
    for (const auto &ext : {".png", ".jpg", ".jpeg", ".webp", ".bmp"}) {
      fs::path p = root / (base + ext);
      if (Exists(p)) return p;
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
    fs::path p = dir / name;
    if (Exists(p)) return p;
  }
  return BestCoverInsideGameFolder(dir);
}

void ScanCoreBucket(std::vector<GameEntry> &out, const fs::path &bucket, CoreKind forced_core,
                    const fs::path &covers_root, const fs::path &alternate_covers_root,
                    const fs::path &saves_root) {
  if (!IsDirectory(bucket)) return;
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(bucket, ec)) {
    if (ec) break;
    if (!entry.is_directory(ec)) continue;
    const fs::path dir = entry.path();
    CoreKind core = forced_core == CoreKind::Unknown ? DetectCore(dir) : forced_core;
    GameEntry game;
    game.core = core;
    game.title = DisplayName(dir);
    game.path = dir;
    game.cover_path = CoverFor(covers_root, alternate_covers_root, dir);
    ReadGameIni(dir, game);
    if (game.core == CoreKind::Unknown) continue;
    game.save_path = saves_root / CoreKindName(game.core) / dir.filename();
    out.push_back(std::move(game));
  }
}
}  // namespace

const char *CoreKindName(CoreKind kind) {
  switch (kind) {
    case CoreKind::Ons: return "ons";
    case CoreKind::Krkr: return "krkr";
    case CoreKind::Tyrano: return "tyrano";
    default: return "unknown";
  }
}

std::vector<GameEntry> ScanGameLibrary(const fs::path &root, const fs::path &games_root,
                                       const fs::path &covers_root, const fs::path &saves_root) {
  const fs::path games = games_root.is_absolute() ? games_root : root / games_root;
  const fs::path covers = covers_root.is_absolute() ? covers_root : root / covers_root;
  const fs::path alternate_covers = covers == root / "game_covers" ? fs::path{} : root / "game_covers";
  const fs::path saves = saves_root.is_absolute() ? saves_root : root / saves_root;

  std::vector<GameEntry> out;
  ScanCoreBucket(out, games / "ons", CoreKind::Ons, covers, alternate_covers, saves);
  ScanCoreBucket(out, games / "krkr", CoreKind::Krkr, covers, alternate_covers, saves);
  ScanCoreBucket(out, games, CoreKind::Unknown, covers, alternate_covers, saves);

  std::sort(out.begin(), out.end(), [](const GameEntry &a, const GameEntry &b) {
    if (a.core != b.core) return static_cast<int>(a.core) < static_cast<int>(b.core);
    return ToLowerAscii(a.title) < ToLowerAscii(b.title);
  });
  out.erase(std::unique(out.begin(), out.end(), [](const GameEntry &a, const GameEntry &b) {
              return a.path == b.path;
            }), out.end());
  return out;
}
