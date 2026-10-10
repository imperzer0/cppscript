cmake_minimum_required(VERSION 3.30)
project(cppscript VERSION 1.5.4 LANGUAGES C CXX DESCRIPTION "Executes a C++ script")

include(FetchContent)

FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.3
        SOURCE_SUBDIR cmake_unofficial
)

FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)

# Configure build options for external dependencies prior to availability
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(xxhash tomlplusplus)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/constants.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/constants.h"
        @ONLY
)

add_executable(cppscript main.cpp wrappers.cpp config.cpp lib.cpp)

# Enable inline mode for xxHash headers
target_compile_definitions(cppscript PRIVATE XXH_INLINE_ALL)
target_include_directories(cppscript PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(cppscript PRIVATE
        xxHash::xxhash
        tomlplusplus::tomlplusplus
)

option(GENERATE_PACKAGE "Whether to generate cpack config or not" OFF)

if (GENERATE_PACKAGE)

    # Standard installation rule
    install(TARGETS cppscript RUNTIME DESTINATION bin)

    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dmytro Perets <dmytroperets@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.31), libstdc++6 (>= 11), g++")
    set(CPACK_GENERATOR "DEB")

    include(CPack)
endif ()