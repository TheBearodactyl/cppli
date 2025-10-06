cmake_minimum_required(VERSION 4.1.1)

project(cppli
    VERSION 1.0.0
    DESCRIPTION "CLI library"
    LANGUAGES CXX
)

add_library(${PROJECT_NAME} SHARED
    src/cppli.cpp
    include/cppli.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
