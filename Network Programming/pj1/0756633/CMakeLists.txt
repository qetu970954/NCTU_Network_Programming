cmake_minimum_required(VERSION 2.8)
message(${CMAKE_VERSION})
project(NetworkProgramming)
set(CMAKE_CXX_STANDARD 14)
set(BOOST_ROOT "/usr/local/include/boost")
find_package(Boost)


add_executable(npshell main.cpp Command.cpp Command.h MyTypeDefines.h Node.h Shell.h)

target_include_directories(npshell
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        )

set_target_properties(npshell
        PROPERTIES
        CMAKE_BUILD_TYPE Release
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")