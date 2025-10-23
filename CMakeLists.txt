cmake_minimum_required(VERSION 4.1.1)

project(
    cppli
    VERSION 1.0.0
    DESCRIPTION "Modern C++ CLI library with strong typing"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    ${PROJECT_NAME}
    STATIC
    src/cppli.cpp
    src/cppli_error.cpp
    src/cppli_types.cpp
	src/cppli_subcommand.cpp
    include/cppli.hpp
    include/cppli_error.hpp
    include/cppli_types.hpp
	include/cppli_subcommand.hpp
)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

#if(MSVC)
#    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
#else()
#    target_compile_options(
#		${PROJECT_NAME}
#        PRIVATE
#            -Wall
#            -Wextra
#            -Wpedantic
#            -Werror
#            -Wconversion
#            -Wsign-conversion
#            -Wnon-virtual-dtor
#            -Wold-style-cast
#            -Wcast-align
#            -Wunused
#            -Woverloaded-virtual
#    )
#endif()

option(CPPLI_BUILD_TESTS "Build tests" ON)

if(CPPLI_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.0
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(
        cppli_tests
        tests/test_error.cpp
        tests/test_types.cpp
        tests/test_parser.cpp
    )

    target_link_libraries(
        cppli_tests
        PRIVATE ${PROJECT_NAME} Catch2::Catch2WithMain
    )

    target_compile_features(cppli_tests PRIVATE cxx_std_23)

    include(CTest)
    include(Catch)
    catch_discover_tests(cppli_tests)
endif()

install(
    TARGETS ${PROJECT_NAME}
    EXPORT cppliTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")

install(
    EXPORT cppliTargets
    FILE cppliTargets.cmake
    NAMESPACE cppli::
    DESTINATION lib/cmake/cppli
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cppliConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cppliConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cppliConfig.cmake"
    INSTALL_DESTINATION lib/cmake/cppli
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cppliConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cppliConfigVersion.cmake"
    DESTINATION lib/cmake/cppli
)
