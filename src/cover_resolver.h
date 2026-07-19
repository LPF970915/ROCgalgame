#pragma once

#include <filesystem>

std::filesystem::path ResolveGameCoverPath(
    const std::filesystem::path &covers_root,
    const std::filesystem::path &alternate_covers_root,
    const std::filesystem::path &game_directory);
