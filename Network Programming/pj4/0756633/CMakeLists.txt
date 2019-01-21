cmake_minimum_required(VERSION 2.8)
project(pj4)
set(CMAKE_CXX_STANDARD 14)

set(BOOST_ROOT /opt/cscc/boost/root/usr/include)

find_package(Boost COMPONENTS REQUIRED system)

set (CMAKE_EXE_LINKER_FLAGS -pthread)

include_directories(${Boost_INCLUDE_DIR})

add_executable(socks_server socks_server.cpp socks4_namespace.hpp)
target_link_libraries(socks_server ${Boost_LIBRARIES})

add_executable(hw4.cgi console.cpp)
target_link_libraries(hw4.cgi ${Boost_LIBRARIES})

