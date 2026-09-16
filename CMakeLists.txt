cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(FileSystemIndexer C CXX)

include(CTest)
enable_testing()

# Build options
option(BUILD_WEB_SERVER "Build with embedded Mongoose HTTP API and Vue Frontend" OFF)

# Default to Debug only if the user didn't specify a build type on the command line
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Per-build-type compiler/sanitizer flags ─────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -mavx2 -mfma -DNDEBUG)    # no sanitizers, no -g, no -O0 in Release
    set(IS_RELEASE_BUILD TRUE)
    set(BUILD_WEB_SERVER OFF)
    set(MOS_LIB_PATH "${CMAKE_SOURCE_DIR}/../MemObjStorage/build-release/libmos.a")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug_Server")
    add_compile_options(-g -O0 -march=native -mavx2 -mfma)
    set(SAN_FLAGS "-fsanitize=address,undefined")

    set(BUILD_WEB_SERVER ON)
    set(IS_RELEASE_BUILD FALSE)
    set(MOS_LIB_PATH "${CMAKE_SOURCE_DIR}/../MemObjStorage/build/libmos.a")
else()
    add_compile_options(-g -O0 -march=native -mavx2 -mfma)

    set(SAN_FLAGS "-fsanitize=address,undefined")
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})

    set(MOS_LIB_PATH "${CMAKE_SOURCE_DIR}/../MemObjStorage/build/libmos.a")
    set(BUILD_WEB_SERVER OFF)
    set(IS_RELEASE_BUILD FALSE)
endif()

if(NOT DEFINED IS_RELEASE_BUILD)
  message(FATAL_ERROR "you need to set IS_RELEASE_BUILD")
endif()

# fetch clip
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
    third_party
    ${clip_cpp_SOURCE_DIR}
    ${clip_cpp_SOURCE_DIR}/ggml/include
)

target_compile_definitions(fsindexer PRIVATE
    FSI_MODEL_DIR="${CMAKE_SOURCE_DIR}/models"
)

# Import the pre-built storage archive as an interface target
add_library(mos_storage STATIC IMPORTED)
set_target_properties(mos_storage PROPERTIES
    IMPORTED_LOCATION "${MOS_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../MemObjStorage/include"
)

# Suppress function pointer runtime checks specifically on GGML to prevent false-positive UBsan traps
if(NOT IS_RELEASE_BUILD)
    target_compile_options(ggml PRIVATE -fno-sanitize=undefined)
endif()

# Link fsindexer dependencies publicly so executables inherit them cleanly
target_link_libraries(fsindexer PUBLIC clip ggml stdc++ mos_storage)

if(BUILD_WEB_SERVER)
    message(STATUS "Building fsindexer with HTTP Server and Web UI")

    # mongoose static lib
    add_library(mongoose STATIC third_party/mongoose/mongoose.c)
    target_include_directories(mongoose PUBLIC third_party/mongoose)

    # mongoose web executable
    add_executable(indexer
        src/main_server.c
        src/server/fsi_server.c
    )

    add_library(cjson STATIC third_party/cJSON/cJSON.c)
    target_include_directories(cjson PUBLIC third_party/cJSON)

    add_library(libb64_cdecode STATIC third_party/b64/cdecode.c)
    target_include_directories(libb64_cdecode PUBLIC third_party/b64)

    # Add compile flag to C code if needed
    target_compile_definitions(indexer PRIVATE ENABLE_WEB_SERVER)

    # Link Core Engine + Web Server
    target_link_libraries(indexer PRIVATE fsindexer mongoose cjson libb64_cdecode)

    # OS-Specific Network Libraries (Windows requires ws2_32)
    if(WIN32)
        target_link_libraries(indexer PRIVATE ws2_32)
    endif()

    target_link_libraries(fsindexer PUBLIC mongoose cjson libb64_cdecode)

    # (Optional) Automatic Web Frontend Embed Rule
    # Reads compiled Vue dist/ files and creates header headers/arrays
    # target_compile_definitions(indexer PRIVATE EMBED_VUE)
else()
    message(STATUS "Building fsindexer headless (CLI/Library only)")

    add_executable(indexer src/main.c) 
    target_link_libraries(indexer PRIVATE fsindexer)
endif()

# clip indexer executable
target_include_directories(indexer PRIVATE ${clip_cpp_SOURCE_DIR})

# Unity Testing
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