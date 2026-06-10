#!/bin/bash
#
# CMM Parser 自动测试脚本 (Lab1)
# 用法: ./run_tests.sh
#
# 对比规则:
#   - 语法树输出: 完全精确对比
#   - 错误输出 (Error type X at Line Y): 仅对比 "Error type X at Line Y" 前缀，
#     冒号后的具体错误描述不参与对比
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARSER="$SCRIPT_DIR/parser"
INPUT_DIR="$SCRIPT_DIR/inputs"
EXPECT_DIR="$SCRIPT_DIR/expects"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RESET='\033[0m'

echo "============================================"
echo "  CMM Parser 自动测试"
echo "============================================"
echo ""

# 检查 parser 是否存在且可执行
if [ ! -x "$PARSER" ]; then
    echo -e "${RED}错误: 找不到 parser 或不可执行: $PARSER${RESET}"
    exit 1
fi

# 规范化输出:
#   1. 对 "Error type X at Line Y:" 开头的行，只保留前缀，去掉冒号及之后的内容
#   2. 将结果写入指定文件，确保末尾有换行符
normalize_to_file() {
    local input="$1"
    local output="$2"

    # 用 awk 处理: 错误行去消息，所有行 print 自动加 \n
    awk '{
        if (match($0, /^Error type [A-Z] at Line [0-9]+:/)) {
            print substr($0, 1, RSTART+RLENGTH-2)
        } else {
            print
        }
    }' "$input" > "$output"

    # 防御: 如果输出文件非空但末尾不是换行符，补一个
    if [ -s "$output" ]; then
        local last_byte
        last_byte=$(tail -c 1 "$output" | od -An -tx1 | tr -d ' ')
        if [ "$last_byte" != "0a" ]; then
            echo "" >> "$output"
        fi
    fi
}

# 收集测试用例，按自然排序 (A-1 在 A-10 前面)
inputs=("$INPUT_DIR"/*.cmm)
mapfile -t inputs < <(printf '%s\n' "${inputs[@]}" | sort -V)

for input_path in "${inputs[@]}"; do
    base="$(basename "$input_path" .cmm)"
    expect_path="$EXPECT_DIR/${base}.exp"
    ((TOTAL++))

    # 没有期望输出文件则跳过
    if [ ! -f "$expect_path" ]; then
        echo -e "${YELLOW}[SKIP]${RESET} $base — 缺少期望输出文件"
        ((SKIP++))
        continue
    fi

    # 运行 parser，stdout/stderr 全部捕获到临时文件
    tmp_raw=$(mktemp)
    "$PARSER" "$input_path" > "$tmp_raw" 2>&1

    # 规范化到独立临时文件
    tmp_actual=$(mktemp)
    tmp_expect=$(mktemp)
    normalize_to_file "$tmp_raw"    "$tmp_actual"
    normalize_to_file "$expect_path" "$tmp_expect"

    # 对比两个规范化后的文件
    if diff -q "$tmp_actual" "$tmp_expect" > /dev/null 2>&1; then
        echo -e "${GREEN}[PASS]${RESET} $base"
        ((PASS++))
    else
        diff_output=$(diff "$tmp_expect" "$tmp_actual" 2>&1 | head -5)
        echo -e "${RED}[FAIL]${RESET} $base — 输出不一致"
        if [ -n "$diff_output" ]; then
            echo "$diff_output" | while IFS= read -r line; do
                echo "         $line"
            done
        fi
        ((FAIL++))
    fi

    rm -f "$tmp_raw" "$tmp_actual" "$tmp_expect"
done

echo ""
echo "============================================"
echo "  测试结果汇总"
echo "============================================"
echo -e "  ${GREEN}通过:${RESET} $PASS"
echo -e "  ${RED}失败:${RESET} $FAIL"
echo -e "  ${YELLOW}跳过:${RESET} $SKIP"
echo -e "  总计: $TOTAL"
echo "============================================"

[ $FAIL -gt 0 ] && exit 1
exit 0
