#!/bin/bash

# 设置颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # 无颜色

# 检查build目录是否存在
BUILD_DIR="build"
BIN_DIR="${BUILD_DIR}/bin"
cd "$BIN_DIR" || {
  echo -e "${RED}错误: 无法进入build目录，请先运行 ./build.sh${NC}"
  exit 1
}
# 检查命令行参数
if [ "$#" -lt 1 ]; then
  echo -e "${YELLOW}用法:${NC}"
  echo -e "  $0 main     ${GREEN}# 运行main_test程序${NC}"
  echo -e "  $0 comp     ${GREEN}# 运行comprehensive_test程序${NC}"
  echo -e "  $0 all      ${GREEN}# 运行所有测试程序${NC}"
  exit 1
fi

# 根据参数运行测试
if [ "$1" = "main" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行main_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  "./main_test"
  if [ $? -ne 0 ]; then
    echo -e "${RED}main_test运行失败${NC}"
  fi
fi

if [ "$1" = "comp" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行comprehensive_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  "./comprehensive_test"
  if [ $? -ne 0 ]; then
    echo -e "${RED}comprehensive_test运行失败${NC}"
  fi
fi

echo -e "${GREEN}所有测试完成${NC}"