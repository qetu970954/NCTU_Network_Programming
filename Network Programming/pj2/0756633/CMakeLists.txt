cmake_minimum_required(VERSION 2.8)
project(NetworkProgramming)

set(CMAKE_CXX_STANDARD 14)
set(BOOST_ROOT "/usr/local/include/boost")
find_package(Boost)

# -----------------------------------------------------------------------
add_executable(np_simple
        simple_main.cpp
        simple_Command.cpp
        simple_Command.h
        simple_MyTypeDefines.h
        simple_Node.h
        simple_Shell.h
        simple_StringManipulator.h
        simple_Pipes.cpp
        )

target_include_directories(np_simple
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        )

# -----------------------------------------------------------------------
add_executable(np_single_proc
        single_proc_main.cpp
        single_proc_MyTypeDefines.h
        single_proc_Node.h
        single_proc_StringManipulator.h
        single_proc_Pipes.h
        )

target_include_directories(np_single_proc
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        )

# -----------------------------------------------------------------------
add_executable(np_multi_proc
        multi_proc_main.cpp
        multi_proc_Command.cpp
        multi_proc_Command.h
        multi_proc_MyTypeDefines.h
        multi_proc_Node.h
        multi_proc_Shell.h
        multi_proc_StringManipulator.h
        multi_proc_Pipes.cpp
        multi_proc_Pipes.h
        multi_proc_SharedMemory.h
        multi_proc_PostOffice.h
        multi_proc_Constants.h
        )

target_include_directories(np_multi_proc
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        )

target_link_libraries(np_multi_proc ${Boost_LIBRARIES} rt -pthread)