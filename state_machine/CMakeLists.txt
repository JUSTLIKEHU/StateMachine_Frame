cmake_minimum_required(VERSION 3.10)

# 库版本信息
set(FSM_VERSION_MAJOR 1)
set(FSM_VERSION_MINOR 0)
set(FSM_VERSION_PATCH 0)
set(FSM_VERSION "${FSM_VERSION_MAJOR}.${FSM_VERSION_MINOR}.${FSM_VERSION_PATCH}")

# 库名称
set(FSM_LIB_NAME statemachine)

# 源文件
set(FSM_SOURCES 
    src/state_machine.cpp
)

# 共享的编译选项
add_compile_options(-Wall -Wextra -fPIC)

# 静态库目标
add_library(${FSM_LIB_NAME}_static STATIC ${FSM_SOURCES})
set_target_properties(${FSM_LIB_NAME}_static PROPERTIES 
    OUTPUT_NAME ${FSM_LIB_NAME}
    POSITION_INDEPENDENT_CODE ON
)
# 修改包含目录设置方式，避免使用PUBLIC关键字与绝对路径
target_include_directories(${FSM_LIB_NAME}_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party>
        $<INSTALL_INTERFACE:include/${FSM_LIB_NAME}>
        $<INSTALL_INTERFACE:include>
)

# 动态库目标
add_library(${FSM_LIB_NAME}_shared SHARED ${FSM_SOURCES})
set_target_properties(${FSM_LIB_NAME}_shared PROPERTIES 
    OUTPUT_NAME ${FSM_LIB_NAME}
    VERSION ${FSM_VERSION}
    SOVERSION ${FSM_VERSION_MAJOR}
)
# 修改包含目录设置方式，避免使用PUBLIC关键字与绝对路径
target_include_directories(${FSM_LIB_NAME}_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party>
        $<INSTALL_INTERFACE:include/${FSM_LIB_NAME}>
        $<INSTALL_INTERFACE:include>
)

# 为简化使用，添加一个别名目标
add_library(FSM::${FSM_LIB_NAME} ALIAS ${FSM_LIB_NAME}_shared)

# 安装规则
include(GNUInstallDirs)

# 安装库文件
install(TARGETS ${FSM_LIB_NAME}_static ${FSM_LIB_NAME}_shared
    EXPORT FSMTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 安装头文件，排除example.h
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${FSM_LIB_NAME}
  FILES_MATCHING PATTERN "*.h"
  PATTERN "*example.h" EXCLUDE
)

# 安装配置文件，允许其他项目使用find_package找到此库
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FSMConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FSMConfig.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/FSMConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSM
)

# 导出目标，使其他项目能够导入
install(EXPORT FSMTargets
    FILE FSMTargets.cmake
    NAMESPACE FSM::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSM
)

# 添加pkg-config支持
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/statemachine.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/statemachine.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/statemachine.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)