cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
project(FileSystemIndexer C CXX)

include(CTest)
enable_testing()

# Default to Debug only if the user didn't specify a build type on the command line
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Per-build-type compiler/sanitizer flags ─────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -mavx2 -mfma -DNDEBUG)
    # no sanitizers, no -g, no -O0 in Release
else()
    add_compile_options(-g -O0 -march=native -mavx2 -mfma)

    set(SAN_FLAGS "-fsanitize=address,undefined")
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})
endif()

include(FetchContent)
FetchContent_Declare(
  clip_cpp
  GIT_REPOSITORY https://github.com/monatis/clip.cpp.git
  GIT_TAG 913458d5d1c9238380a0b0826cd1e71c8828a82d
  GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(clip_cpp)

# main library
add_library(fsindexer STATIC 
    src/fsi_crawler.c
    src/fsi_indexer.c
    src/fsi_os.c
    src/fsi_queue.c
    src/fsi_watcher.c
    src/fsi_worker.c
    src/thr_thread.c
    src/fsi_clip.c)
target_include_directories(fsindexer PUBLIC 
    src 
    ${clip_cpp_SOURCE_DIR}
    ${clip_cpp_SOURCE_DIR}/ggml/include
)

target_compile_definitions(fsindexer PRIVATE
    FSI_MODEL_DIR="${CMAKE_SOURCE_DIR}/models"
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(MOS_LIB_PATH "${CMAKE_SOURCE_DIR}/../MemObjStorage/build-release/libmos.a")
else()
    set(MOS_LIB_PATH "${CMAKE_SOURCE_DIR}/../MemObjStorage/build/libmos.a")
endif()

# ── Import the pre-built storage archive as an interface target ───────────────
add_library(mos_storage STATIC IMPORTED)
set_target_properties(mos_storage PROPERTIES
    IMPORTED_LOCATION "${MOS_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../MemObjStorage/include"
)

# Suppress function pointer runtime checks specifically on GGML to prevent false-positive UBsan traps
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(ggml PRIVATE -fno-sanitize=undefined)
endif()

# Link fsindexer dependencies publicly so executables inherit them cleanly
target_link_libraries(fsindexer PUBLIC clip ggml stdc++ mos_storage)

# ── indexer executable ───────────────────────────────────────────────────
add_executable(indexer src/main.c) 
target_include_directories(indexer PRIVATE ${clip_cpp_SOURCE_DIR})
target_link_libraries(indexer PRIVATE fsindexer)

# ── Unity Testing ──────────────────────────────────────────────────────────
add_library(unity STATIC tests/unity.c)
target_include_directories(unity PUBLIC tests)

set(TEST_SOURCES
    tests/fsi_test_queue.c
    tests/fsi_test_crawler.c
)

set(FUZZY_TEST_SOURCES
    tests/fsi_fuzzy_crawler.c
    tests/fsi_fuzzy_worker.c
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE unity fsindexer mos_storage)
    target_compile_definitions(${test_name} PRIVATE
        FSI_MODEL_DIR="${CMAKE_SOURCE_DIR}/models"
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${FUZZY_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE fsindexer mos_storage)
    target_compile_definitions(${test_name} PRIVATE
        FSI_MODEL_DIR="${CMAKE_SOURCE_DIR}/models"
    )
    target_compile_options(${test_name} PRIVATE -g -fsanitize=fuzzer,address,undefined)
    target_link_options(${test_name} PRIVATE -fsanitize=fuzzer,address,undefined)
    add_test(NAME ${test_name} COMMAND ${test_name} -runs=10000 -max_len=65536 -len_control=0 -print_final_stats=1)
endforeach()