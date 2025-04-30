# 多范围条件测试
add_executable(multi_range_conditions_test 
    test_multi_range_conditions.cpp
)

# 链接库
target_link_libraries(multi_range_conditions_test PRIVATE statemachine_static)

# 设置可执行文件输出路径
set_target_properties(multi_range_conditions_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
) 