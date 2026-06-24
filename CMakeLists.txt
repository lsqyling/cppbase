cmake_minimum_required(VERSION 3.25)
project(cppbase
        VERSION 1.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
endif ()


add_library(cppbase INTERFACE)
target_include_directories(cppbase
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_subdirectory(extern/benchmark)
add_subdirectory(test)