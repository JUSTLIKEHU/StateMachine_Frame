
find_package(Threads REQUIRED)

find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(multi_event_test multi_event_test.cpp)
target_link_libraries(multi_event_test
    PRIVATE
    statemachine_static
    gtest
    gtest_main
    Threads::Threads
)

# 设置可执行文件输出路径
set_target_properties(multi_event_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME multi_event_test COMMAND multi_event_test) 