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

if [ ! -d "$BIN_DIR" ]; then
  echo -e "${RED}错误: 找不到bin目录，请先运行 ./build.sh test${NC}"
  exit 1
fi

cd "$BIN_DIR" || {
  echo -e "${RED}错误: 无法进入build/bin目录，请先运行 ./build.sh test${NC}"
  exit 1
}

# 检查测试可执行文件是否存在
if [ ! -f "./main_test" ] && [ ! -f "./comprehensive_test" ] && [ ! -f "./condition_event_test" ] && \
   [ ! -f "./multi_range_conditions_test" ] && [ ! -f "./state_timeout_test" ] && [ ! -f "./multi_event_test" ]; then
  echo -e "${RED}错误: 找不到测试可执行文件，请先运行 ./build.sh test 来构建测试${NC}"
  cd ../../
  exit 1
fi

# 检查命令行参数
if [ "$#" -lt 1 ]; then
  echo -e "${YELLOW}用法:${NC}"
  echo -e "  $0 main     ${GREEN}# 运行main_test程序${NC}"
  echo -e "  $0 comp     ${GREEN}# 运行comprehensive_test程序${NC}"
  echo -e "  $0 condition ${GREEN}# 运行condition_event_test程序${NC}"
  echo -e "  $0 multi     ${GREEN}# 运行multi_range_conditions_test程序${NC}"
  echo -e "  $0 timeout   ${GREEN}# 运行state_timeout_test程序${NC}"
  echo -e "  $0 event     ${GREEN}# 运行multi_event_test程序${NC}"
  echo -e "  $0 all      ${GREEN}# 运行所有测试程序${NC}"
  exit 1
fi

# 根据参数运行测试
if [ "$1" = "main" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行main_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./main_test" ]; then
    echo -e "${RED}main_test不存在${NC}"
  else
    "./main_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}main_test运行失败${NC}"
    fi
  fi
fi

if [ "$1" = "comp" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行comprehensive_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./comprehensive_test" ]; then
    echo -e "${RED}comprehensive_test不存在${NC}"
  else
    "./comprehensive_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}comprehensive_test运行失败${NC}"
    fi
  fi
fi

if [ "$1" = "condition" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行condition_event_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./condition_event_test" ]; then
    echo -e "${RED}condition_event_test不存在${NC}"
  else
    "./condition_event_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}condition_event_test运行失败${NC}"
    fi
  fi
fi

if [ "$1" = "multi" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行multi_range_conditions_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./multi_range_conditions_test" ]; then
    echo -e "${RED}multi_range_conditions_test不存在${NC}"
  else
    "./multi_range_conditions_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}multi_range_conditions_test运行失败${NC}"
    fi
  fi
fi

if [ "$1" = "timeout" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行state_timeout_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./state_timeout_test" ]; then
    echo -e "${RED}state_timeout_test不存在${NC}"
  else
    "./state_timeout_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}state_timeout_test运行失败${NC}"
    fi
  fi
fi

if [ "$1" = "event" ] || [ "$1" = "all" ]; then
  echo -e "${BLUE}===============================${NC}"
  echo -e "${BLUE}运行multi_event_test...${NC}"
  echo -e "${BLUE}===============================${NC}"
  if [ ! -f "./multi_event_test" ]; then
    echo -e "${RED}multi_event_test不存在${NC}"
  else
    "./multi_event_test"
    if [ $? -ne 0 ]; then
      echo -e "${RED}multi_event_test运行失败${NC}"
    fi
  fi
fi

cd ../../
echo -e "${GREEN}所有测试完成${NC}"