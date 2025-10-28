cmake_minimum_required(VERSION 3.31)
project(cpp_vector)

set(CMAKE_CXX_STANDARD 20)

add_executable(cpp_vector main.cpp
        vector.h
        rawmemory.h
)
