cmake_minimum_required(VERSION 3.10)
project(StateMachine_Frame VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加编译选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 防止源代码内构建
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "不允许在源代码目录内构建。请创建一个单独的构建目录并从那里运行CMake。")
endif()

# 添加 state_machine 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/state_machine/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 设置安装前缀（如果未指定）
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "安装前缀" FORCE)
endif()

# 包含GNUInstallDirs模块以获取跨平台的安装目录变量
include(GNUInstallDirs)

# 添加子目录
add_subdirectory(state_machine)
add_subdirectory(test)

# 配置版本文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FSMConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

# 安装版本文件
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/FSMConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FSM
)

# 安装第三方库头文件
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
