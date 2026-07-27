#include <filesystem>
#include <iostream>

int main() {
  const std::filesystem::path marker("krkr2-aarch64-toolchain");
  std::cout << marker.string() << '\n';
  return marker.empty() ? 1 : 0;
}
