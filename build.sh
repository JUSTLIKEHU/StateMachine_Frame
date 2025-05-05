#!/bin/bash

# 设置颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # 无颜色

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

# 进入build目录
cd "$BUILD_DIR" || exit 1

# 运行CMake
echo -e "${BLUE}配置工程...${NC}"
cmake .. -DBUILD_TESTS=${BUILD_TESTS}

# 编译项目
echo -e "${BLUE}编译项目...${NC}"
make -j$(nproc)

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