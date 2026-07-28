cmake_minimum_required(VERSION 3.20)
project(krkr2_toolchain_probe LANGUAGES C CXX)

add_executable(krkr2_toolchain_probe main.cpp)
target_compile_features(krkr2_toolchain_probe PRIVATE cxx_std_17)
