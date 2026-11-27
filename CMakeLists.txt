cmake_minimum_required(VERSION 3.20)
project(arm_recomp_core LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArmRecompCore.cmake")

# Consumers own their runtime ABI and platform integration, so this repository
# is primarily a source package. The interface target exposes common headers for
# small tools/tests that only need the architectural types.
add_library(arm_recomp_core_headers INTERFACE)
target_include_directories(arm_recomp_core_headers INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/common")

option(ARM_RECOMP_CORE_BUILD_TESTS "Build common-core storage contract tests" OFF)
if(ARM_RECOMP_CORE_BUILD_TESTS)
    enable_testing()
    arm_recomp_core_sources(common_test_sources armv4t)
    add_executable(packed_state_test tests/packed_state_test.cpp ${common_test_sources})
    target_link_libraries(packed_state_test PRIVATE arm_recomp_core_headers)
    target_compile_features(packed_state_test PRIVATE cxx_std_20)
    add_test(NAME packed_state_test COMMAND packed_state_test)
    add_test(NAME exception_return_structured_test COMMAND packed_state_test --exception-return-structured)
    add_test(NAME exception_return_packed_test COMMAND packed_state_test --exception-return-packed)
endif()

# Compile the native generation profiles independently of a console runtime.
foreach(profile armv4t_gba armv5te_nds)
    arm_recomp_core_sources(profile_sources ${profile})
    list(FILTER profile_sources EXCLUDE REGEX "interpreter[.]cpp$")
    arm_recomp_core_include_dirs(profile_includes ${profile})
    add_library(arm_recomp_${profile} STATIC ${profile_sources})
    target_include_directories(arm_recomp_${profile} PUBLIC ${profile_includes})
    target_compile_features(arm_recomp_${profile} PUBLIC cxx_std_20)
endforeach()
