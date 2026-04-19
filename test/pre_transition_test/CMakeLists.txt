cmake_minimum_required(VERSION 3.10)

# 添加 pre-transition 单元测试可执行文件
add_executable(pre_transition_test main.cpp)

# 设置包含目录
target_include_directories(pre_transition_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

# 链接线程库与状态机静态库
find_package(Threads REQUIRED)
target_link_libraries(pre_transition_test PRIVATE statemachine_static Threads::Threads)

# 设置可执行文件输出路径
set_target_properties(pre_transition_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装规则
install(TARGETS pre_transition_test DESTINATION bin)

# 复制配置文件到输出目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config
     DESTINATION ${CMAKE_BINARY_DIR}/bin/test/pre_transition_test)
