#pragma once

#include <string>

namespace gkd350h_ultra {

constexpr int kScreenW = 1600;
constexpr int kScreenH = 1440;
constexpr int kBodyFontPt = 32;
constexpr int kTitleFontPt = 48;
constexpr int kMenuBodyFontPt = 44;
constexpr int kMenuTitleFontPt = 66;
constexpr int kFontScale = 2;
constexpr const char *kProfileName = "1600x1440";
constexpr const char *kCanonicalModel = "gkd350h-ultra";

inline bool IsProfileName(const std::string &profile) {
  return profile == kProfileName || profile == "gkd350h-ultra" || profile == "gkd350h" ||
         profile == "gkd-ultra" || profile == "gkd_atom" || profile == "gkd-atom" ||
         profile == "gamekiddy-gkd-atom";
}

inline bool IsModelAliasCompact(const std::string &compact) {
  return compact.find("gkd350hultra") != std::string::npos ||
         compact.find("gkd350h") != std::string::npos ||
         compact.find("gamekiddygkdatom") != std::string::npos ||
         compact.find("gkdatom") != std::string::npos;
}

inline bool IsCanonicalModel(const std::string &model_token) {
  return model_token == kCanonicalModel;
}

}  // namespace gkd350h_ultra
