cmake_minimum_required(VERSION 3.10)

# 添加条件事件测试可执行文件
add_executable(condition_event_test condition_event_test.cpp)

# 设置包含目录
target_include_directories(condition_event_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

# 链接线程库
find_package(Threads REQUIRED)
# 修改为正确的库名称
target_link_libraries(condition_event_test PRIVATE statemachine_static Threads::Threads)

# 设置可执行文件输出路径
set_target_properties(condition_event_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装规则
install(TARGETS condition_event_test DESTINATION bin)