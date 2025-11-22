/**
 * @mainpage CPPli Documentation
 *
 * @subsection features Cool Features
 * - Automatic help generation
 * - Fluent API for adding flags, positionals, etc.
 *
 * @subsection setup_section Setup
 * Add the below to your projects `CMakeLists.txt`:
 * ~~~{.cmake}
 * include(FetchContent)
 *
 * FetchContent_Declare(cppli GIT_REPOSITORY "https://github.com/thebearodactyl/cppli" GIT_TAG "main")
 * FetchContent_MakeAvailable(cppli)
 *
 * // after your `add_executable` or `add_library`
 * target_link_libraries(${PROJECT_NAME} cppli)
 * ~~~
 *
 * @subsection todos_section TODOS
 *
 * Features:
 * - [x] Add subcommand support
 *
 * Fixes:
 * - [x] Fix the auto-generated help message not being colored
 */
