cmake_minimum_required(VERSION 3.10)
project(FileSystemIndexer C)

# ── indexer executable ───────────────────────────────────────────────────
file(GLOB APP_SRCS src/*.c)
add_executable(indexer ${APP_SRCS})

# ── Import the pre-built storage archive as an interface target ───────────────
add_library(mos_storage STATIC IMPORTED)
set_target_properties(mos_storage PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../MemObjStorage/build/libmos.a"
)

# ── Wire headers and archive into the indexer ─────────────────────────────────
target_include_directories(indexer PRIVATE
    ${CMAKE_SOURCE_DIR}/../MemObjStorage/include
)
target_link_libraries(indexer PRIVATE mos_storage)