cmake_minimum_required(VERSION 3.3)
project(ThreadsOfDoom)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp example.cpp)
add_executable(ThreadsOfDoom ${SOURCE_FILES})