#pragma once

#include "game_library.h"

#include <filesystem>
#include <vector>

std::vector<GameEntry> ScanGameLibrary(const std::filesystem::path &root,
                                       const std::filesystem::path &games_root,
                                       const std::filesystem::path &covers_root,
                                       const std::filesystem::path &saves_root);
