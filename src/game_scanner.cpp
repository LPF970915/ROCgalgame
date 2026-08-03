#include "game_scanner.h"

#include "config.h"
#include "cover_resolver.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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

bool HasXp3Signature(const fs::path &path) {
  static constexpr std::array<unsigned char, 11> kMagic = {
      'X', 'P', '3', '\r', '\n', ' ', '\n', 0x1a, 0x8b, 0x67, 0x01};
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::array<unsigned char, kMagic.size()> header{};
  in.read(reinterpret_cast<char *>(header.data()),
          static_cast<std::streamsize>(header.size()));
  return in.gcount() == static_cast<std::streamsize>(header.size()) &&
         header == kMagic;
}

constexpr std::uint64_t kMaxXp3IndexBytes = 64ull * 1024ull * 1024ull;

std::uint64_t ReadLe64(const unsigned char *data) {
  std::uint64_t value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8)
    value |= static_cast<std::uint64_t>(*data++) << shift;
  return value;
}

std::uint16_t ReadLe16(const unsigned char *data) {
  return static_cast<std::uint16_t>(data[0]) |
         static_cast<std::uint16_t>(data[1] << 8);
}

using ZlibUncompress = int (*)(unsigned char *, unsigned long *,
                               const unsigned char *, unsigned long);

ZlibUncompress ResolveZlibUncompress() {
  static ZlibUncompress function = []() -> ZlibUncompress {
#ifdef _WIN32
    HMODULE library = LoadLibraryA("zlib1.dll");
    return library ? reinterpret_cast<ZlibUncompress>(
                         GetProcAddress(library, "uncompress"))
                   : nullptr;
#else
    void *library = dlopen("libz.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!library) library = dlopen("libz.so", RTLD_LAZY | RTLD_LOCAL);
    return library ? reinterpret_cast<ZlibUncompress>(
                         dlsym(library, "uncompress"))
                   : nullptr;
#endif
  }();
  return function;
}

bool ReadXp3Index(const fs::path &path, std::vector<unsigned char> &index) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::array<unsigned char, 19> header{};
  in.read(reinterpret_cast<char *>(header.data()),
          static_cast<std::streamsize>(header.size()));
  if (in.gcount() != static_cast<std::streamsize>(header.size())) return false;
  static constexpr std::array<unsigned char, 11> kMagic = {
      'X', 'P', '3', '\r', '\n', ' ', '\n', 0x1a, 0x8b, 0x67, 0x01};
  if (!std::equal(kMagic.begin(), kMagic.end(), header.begin())) return false;
  const std::uint64_t index_offset = ReadLe64(header.data() + kMagic.size());
  if (index_offset > static_cast<std::uint64_t>(
                         std::numeric_limits<std::streamoff>::max()))
    return false;
  in.seekg(static_cast<std::streamoff>(index_offset));
  if (!in) return false;

  index.clear();
  for (int block = 0; block < 32; ++block) {
    unsigned char flags = 0;
    std::array<unsigned char, 8> raw_size{};
    in.read(reinterpret_cast<char *>(&flags), 1);
    in.read(reinterpret_cast<char *>(raw_size.data()), 8);
    if (!in) return false;
    const std::uint64_t archive_size = ReadLe64(raw_size.data());
    std::uint64_t original_size = archive_size;
    if (flags & 0x01) {
      in.read(reinterpret_cast<char *>(raw_size.data()), 8);
      if (!in) return false;
      original_size = ReadLe64(raw_size.data());
    }
    if (archive_size > kMaxXp3IndexBytes || original_size > kMaxXp3IndexBytes ||
        index.size() + original_size > kMaxXp3IndexBytes)
      return false;

    std::vector<unsigned char> archived(static_cast<size_t>(archive_size));
    in.read(reinterpret_cast<char *>(archived.data()),
            static_cast<std::streamsize>(archived.size()));
    if (!in) return false;
    if (flags & 0x01) {
      const auto uncompress = ResolveZlibUncompress();
      if (!uncompress ||
          archive_size > std::numeric_limits<unsigned long>::max() ||
          original_size > std::numeric_limits<unsigned long>::max())
        return false;
      std::vector<unsigned char> inflated(static_cast<size_t>(original_size));
      unsigned long inflated_size = static_cast<unsigned long>(original_size);
      if (uncompress(inflated.data(), &inflated_size, archived.data(),
                     static_cast<unsigned long>(archive_size)) != 0 ||
          inflated_size != original_size)
        return false;
      index.insert(index.end(), inflated.begin(), inflated.end());
    } else {
      index.insert(index.end(), archived.begin(), archived.end());
    }
    if (!(flags & 0x80)) return true;
  }
  return false;
}

bool IsRootStartupName(const unsigned char *data, size_t byte_count) {
  if (byte_count % 2 != 0) return false;
  std::string name;
  name.reserve(byte_count / 2);
  for (size_t offset = 0; offset < byte_count; offset += 2) {
    const std::uint16_t value = ReadLe16(data + offset);
    if (value > 0x7f) return false;
    char ch = static_cast<char>(value);
    if (ch == '\\') ch = '/';
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
    name.push_back(ch);
  }
  while (!name.empty() && name.front() == '/') name.erase(name.begin());
  while (name.rfind("./", 0) == 0) name.erase(0, 2);
  return name == "startup.tjs";
}

bool Xp3IndexContainsStartup(const std::vector<unsigned char> &index) {
  size_t offset = 0;
  while (offset + 12 <= index.size()) {
    const std::uint64_t chunk_size = ReadLe64(index.data() + offset + 4);
    const size_t payload = offset + 12;
    if (chunk_size > index.size() - payload) return false;
    const size_t chunk_end = payload + static_cast<size_t>(chunk_size);
    if (std::memcmp(index.data() + offset, "File", 4) == 0) {
      size_t sub = payload;
      while (sub + 12 <= chunk_end) {
        const std::uint64_t sub_size = ReadLe64(index.data() + sub + 4);
        const size_t sub_payload = sub + 12;
        if (sub_size > chunk_end - sub_payload) return false;
        const size_t sub_end = sub_payload + static_cast<size_t>(sub_size);
        if (std::memcmp(index.data() + sub, "info", 4) == 0 && sub_size >= 22) {
          const std::uint16_t name_length =
              ReadLe16(index.data() + sub_payload + 20);
          const size_t name_bytes = static_cast<size_t>(name_length) * 2;
          if (name_bytes <= sub_end - (sub_payload + 22) &&
              IsRootStartupName(index.data() + sub_payload + 22, name_bytes))
            return true;
        }
        sub = sub_end;
      }
    }
    offset = chunk_end;
  }
  return false;
}

bool HasXp3StartupScript(const fs::path &path) {
  std::vector<unsigned char> index;
  return ReadXp3Index(path, index) && Xp3IndexContainsStartup(index);
}

bool HasXp3SignatureInDirectory(const fs::path &dir) {
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) {
      ec.clear();
      continue;
    }
    if (HasXp3Signature(entry.path())) return true;
  }
  return false;
}

bool IsPatchArchive(const fs::path &path) {
  const std::string stem = ToLowerAscii(path.stem().u8string());
  return stem.rfind("patch", 0) == 0;
}

fs::path DetectKrkrEntryPoint(const fs::path &dir) {
  std::vector<fs::path> archives;
  std::vector<fs::path> startup_archives;
  fs::path data_archive;
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) {
      ec.clear();
      continue;
    }
    const std::string filename = ToLowerAscii(entry.path().filename().u8string());
    if (filename == "startup.tjs" || filename == "data.xp3") return dir;
    if (!HasXp3Signature(entry.path()) || IsPatchArchive(entry.path())) continue;
    archives.push_back(entry.path());
    if (HasXp3StartupScript(entry.path())) startup_archives.push_back(entry.path());
    if (ToLowerAscii(entry.path().stem().u8string()) == "data") {
      data_archive = entry.path();
    }
  }
  if (!data_archive.empty()) return data_archive;
  if (startup_archives.size() == 1) return startup_archives.front();
  if (archives.size() == 1) return archives.front();
  return dir;
}

CoreKind DetectCore(const fs::path &dir) {
  if (HasAny(dir, {"0.txt", "00.txt", "nscript.dat", "nscript.___", "arc.nsa", "arc.sar"})) return CoreKind::Ons;
  if (HasAny(dir, {"startup.tjs", "Config.tjs", "config.tjs", "data.xp3"}) ||
      HasAnyExtension(dir, {".xp3"}) || HasXp3SignatureInDirectory(dir)) {
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

KrkrRuntime ParseKrkrRuntime(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  if (v == "sdl2" || v == "krkrsdl2" || v == "fast") return KrkrRuntime::Sdl2;
  if (v == "krkr2" || v == "kirikiroid2" || v == "native") return KrkrRuntime::Krkr2;
  if (v == "wine" || v == "windows") return KrkrRuntime::Wine;
  return KrkrRuntime::Auto;
}

bool IsAspectValue(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  return v == "stretch" || v == "contain" || v == "fill-height" ||
         v == "fill-width" || v == "fit-width";
}

bool IsFilterValue(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  return v == "clean" || v == "antialias" || v == "scanline" ||
         v == "dot" || v == "reflection" || v == "crt-soft" || v == "mask";
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
    } else if (key == "entry" && !value.empty()) {
      game.overrides.entry = value;
    } else if (key == "runtime" || key == "krkr_runtime") {
      game.overrides.krkr_runtime = ParseKrkrRuntime(value);
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
    } else if (key == "frame_limit") {
      try { game.overrides.frame_limit = std::clamp(std::stoi(value), 1, 240); } catch (...) {}
    } else if (key == "draw_threads") {
      const std::string threads = ToLowerAscii(value);
      if (threads == "auto" || threads == "1" || threads == "2" || threads == "4") game.overrides.draw_threads = threads;
    } else if (key == "graphic_cache_mb") {
      try { game.overrides.graphic_cache_mb = std::clamp(std::stoi(value), 16, 512); } catch (...) {}
    }
  }
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
    game.cover_path = ResolveGameCoverPath(covers_root, alternate_covers_root, dir);
    ReadGameIni(dir, game);
    if (game.core == CoreKind::Unknown) continue;
    if (game.core == CoreKind::Krkr) {
      if (!game.overrides.entry.empty()) {
        fs::path configured = fs::u8path(game.overrides.entry);
        game.entry_point = configured.is_absolute() ? configured : dir / configured;
      } else {
        game.entry_point = DetectKrkrEntryPoint(dir);
      }
      const fs::path standard_data_archive = dir / "data.xp3";
      if (game.overrides.krkr_runtime == KrkrRuntime::Auto &&
          ((game.entry_point != dir && HasXp3Signature(game.entry_point)) ||
           HasXp3Signature(standard_data_archive))) {
        game.overrides.krkr_runtime = KrkrRuntime::Krkr2;
      }
    }
    game.save_path = saves_root / CoreKindName(game.core) / dir.filename();
    out.push_back(std::move(game));
  }
}
}  // namespace

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
