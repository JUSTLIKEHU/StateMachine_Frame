cmake_minimum_required(VERSION 3.10)

project(StateMachineFrame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(third_party)

add_subdirectory(src)
