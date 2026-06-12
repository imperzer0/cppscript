cmake_minimum_required(VERSION 4.2)
project(cppscript LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG        v0.8.3
)

FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(xxhash)
FetchContent_MakeAvailable(tomlplusplus)

set(CMAKE_CXX_STANDARD 26)

add_executable(cppscript main.cpp)

target_link_libraries(cppscript PRIVATE xxhash tomlplusplus::tomlplusplus)
