# 添加主测试可执行文件
add_executable(main_test 
    main_test/main_test.cpp
)

# 添加全面测试可执行文件
add_executable(comprehensive_test 
    comprehensive_test/comprehensive_test.cpp
)

# 添加测试状态子状态结构文件
add_executable(state_hierarchy_test 
    comprehensive_test/state_hierarchy_test.cpp
)

# 添加条件事件测试子目录
add_subdirectory(conditions_event)

# 设置线程库
find_package(Threads REQUIRED)

# 链接库 - 修改为正确的库名称
target_link_libraries(main_test PRIVATE statemachine_static Threads::Threads)
target_link_libraries(comprehensive_test PRIVATE statemachine_static Threads::Threads)
target_link_libraries(state_hierarchy_test PRIVATE statemachine_static Threads::Threads)

# 设置可执行文件输出路径
set_target_properties(main_test comprehensive_test state_hierarchy_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)