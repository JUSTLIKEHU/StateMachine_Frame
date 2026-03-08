cmake_minimum_required(VERSION 3.10)

project(condition_expr_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(condition_expr_test condition_expr_test.cpp)

target_include_directories(condition_expr_test PRIVATE
    ${CMAKE_SOURCE_DIR}/state_machine/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(condition_expr_test PRIVATE
    statemachine_static
    pthread
)

# 设置可执行文件输出路径
set_target_properties(condition_expr_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 复制配置文件到bin目录
add_custom_command(TARGET condition_expr_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_BINARY_DIR}/bin/condition_expr_test_config
)
