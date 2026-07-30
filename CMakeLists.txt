cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER "clang")
project(FileSystemIndexer C)

include(CTest)
enable_testing()

set(CMAKE_BUILD_TYPE Debug)

# main library
add_library(fsindexer STATIC 
    src/fsi_crawler.c
    src/fsi_indexer.c
    src/fsi_os.c
    src/fsi_queue.c
    src/fsi_watcher.c
    src/fsi_worker.c
    src/thr_thread.c)
target_include_directories(fsindexer PUBLIC src)

# ── indexer executable ───────────────────────────────────────────────────
add_executable(indexer src/main.c) 

# ── Import the pre-built storage archive as an interface target ───────────────
add_library(mos_storage STATIC IMPORTED)
set_target_properties(mos_storage PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../MemObjStorage/build/libmos.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../MemObjStorage/include"
)

# Unity as static library
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

# Loop through each test file, creating a distinct executable for it
foreach(test_file ${TEST_SOURCES})
    # Get the file name without path or extension (e.g., "fsi_test_queue")
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # Create the target executable
    add_executable(${test_name} ${test_file})
    
    # Link dependencies
    target_link_libraries(${test_name} PRIVATE unity fsindexer mos_storage)
    
    # Register it with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${FUZZY_TEST_SOURCES})
    # Get the file name without path or extension (e.g., "fsi_fuzzy_crawler")
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # Create the target executable
    add_executable(${test_name} ${test_file})
    
    # Link dependencies
    target_link_libraries(${test_name} PRIVATE fsindexer mos_storage)
    
    target_compile_options(${test_name} PRIVATE -g -fsanitize=fuzzer,address,undefined)
    target_link_options(${test_name} PRIVATE -fsanitize=fuzzer,address,undefined)

    add_test(NAME ${test_name} COMMAND ${test_name} -runs=10000 -max_len=65536 -len_control=0 -print_final_stats=1)
endforeach()

target_link_libraries(fsindexer PRIVATE mos_storage)
target_link_libraries(indexer PRIVATE fsindexer mos_storage)
