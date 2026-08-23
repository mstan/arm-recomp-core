cmake_minimum_required(VERSION 3.20)
project(arm_recomp_core LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArmRecompCore.cmake")

# Consumers own their runtime ABI and platform integration, so this repository
# is primarily a source package. The interface target exposes common headers for
# small tools/tests that only need the architectural types.
add_library(arm_recomp_core_headers INTERFACE)
target_include_directories(arm_recomp_core_headers INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/common")
