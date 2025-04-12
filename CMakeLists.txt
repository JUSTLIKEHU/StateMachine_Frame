cmake_minimum_required(VERSION 3.10)
project(StateMachine_Frame VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加 state_machine 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/state_machine)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 添加测试目录
add_subdirectory(test)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/state_machine
    DESTINATION include
)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    DESTINATION include
)
