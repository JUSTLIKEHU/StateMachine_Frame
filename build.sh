#!/bin/bash

# 设置颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # 无颜色

# 检查必要的命令是否存在
check_command() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}错误: 未找到 $1 命令${NC}"
        echo -e "${YELLOW}请安装 $1 后再运行此脚本${NC}"
        exit 1
    fi
}

# 检查依赖
check_command cmake
check_command make

# 获取CPU核心数
if command -v nproc &> /dev/null; then
    CPU_CORES=$(nproc)
else
    CPU_CORES=4  # 默认值
    echo -e "${YELLOW}警告: 未找到 nproc 命令，使用默认值 4 个核心${NC}"
fi

# 初始化变量
BUILD_TESTS=OFF

# 处理命令行参数
if [ "$1" = "test" ]; then
  echo -e "${BLUE}启用测试构建...${NC}"
  BUILD_TESTS=ON
fi

# 检查目录
BUILD_DIR="build"
BIN_DIR="${BUILD_DIR}/bin"

# 创建目录如果不存在
if [ ! -d "$BUILD_DIR" ]; then
  echo -e "${BLUE}创建build目录...${NC}"
  mkdir -p "$BUILD_DIR"
fi

# 确保脚本有执行权限
chmod +x "$0"

# 进入build目录
cd "$BUILD_DIR" || {
    echo -e "${RED}错误: 无法进入 build 目录${NC}"
    exit 1
}

# 运行CMake
echo -e "${BLUE}配置工程...${NC}"
cmake .. -DBUILD_TESTS=${BUILD_TESTS} || {
    echo -e "${RED}错误: CMake 配置失败${NC}"
    exit 1
}

# 编译项目
echo -e "${BLUE}编译项目...${NC}"
make -j${CPU_CORES} || {
    echo -e "${RED}错误: 编译失败${NC}"
    exit 1
}

# 检查编译是否成功
if [ $? -eq 0 ]; then
  echo -e "${GREEN}编译成功！${NC}"
  echo -e "${YELLOW}可执行文件位于: ${BIN_DIR}${NC}"
  # 列出生成的可执行文件
  if [ -d "$BIN_DIR" ]; then
    echo -e "${BLUE}生成的可执行文件:${NC}"
    ls -la "$BIN_DIR"
  fi
  
  # 显示如何运行测试的提示
  if [ "$BUILD_TESTS" = "ON" ]; then
    echo -e "${YELLOW}要运行测试，请使用: ./run_test.sh [选项]${NC}"
  fi
else
  echo -e "${RED}编译失败！${NC}"
  exit 1
fi

cd ..