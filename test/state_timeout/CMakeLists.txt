project(state_timeout_test)

# 查找线程库
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} state_timeout_test.cpp)

target_link_libraries(${PROJECT_NAME} statemachine_static Threads::Threads)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 复制配置文件到输出目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config 
     DESTINATION ${CMAKE_BINARY_DIR}/bin/test/state_timeout) 