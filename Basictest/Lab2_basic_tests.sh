#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARSER="$ROOT_DIR/Code/parser"
CASE_DIR="${1:-$ROOT_DIR/Basictest/Lab2}"

if [[ ! -x "$PARSER" ]]; then
  echo "错误: 找不到可执行文件 $PARSER"
  echo "请先在 [Code/Makefile](Code/Makefile) 所在目录编译 parser。"
  exit 1
fi

if [[ ! -d "$CASE_DIR" ]]; then
  echo "错误: 测试目录不存在: $CASE_DIR"
  exit 1
fi

found=0
while IFS= read -r -d '' file; do
  found=1
  rel="${file#$ROOT_DIR/}"
  echo "========== $rel =========="
  "$PARSER" "$file"
  echo
done < <(find "$CASE_DIR" -maxdepth 1 -type f -name '*.cmm' -print0 | sort -z -V)

if [[ $found -eq 0 ]]; then
  echo "未找到 .cmm 文件: $CASE_DIR"
  exit 1
fi