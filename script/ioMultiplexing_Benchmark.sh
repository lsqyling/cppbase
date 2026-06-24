#!/bin/bash
# IO Multiplexing Benchmark Script
# 逐步增加并发数，测试 select/poll/epoll 三种模型的性能对比

# 自动定位项目根目录和数据目录
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DATA_DIR="$PROJECT_ROOT/data"

HOST="127.0.0.1"
TOTAL_REQUESTS=100000
TIMEOUT_SEC=120

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}==========================================${NC}"
echo -e "${YELLOW}IO多路复用性能对比测试${NC}"
echo -e "${YELLOW}==========================================${NC}"
echo "目标请求数: $TOTAL_REQUESTS"
echo "单个测试超时: ${TIMEOUT_SEC}s"
echo ""

# 检查 redis-benchmark 是否可用
if ! command -v redis-benchmark &> /dev/null; then
    echo -e "${RED}错误: redis-benchmark 未安装${NC}"
    echo "请先安装 Redis: sudo apt-get install redis-tools (Ubuntu) 或 brew install redis (Mac)"
    exit 1
fi

# 提升进程文件描述符上限（redis-benchmark 创建 2000+ 连接也需要足够 fd）
FD_LIMIT=$(ulimit -n)
echo -e "${YELLOW}当前 fd 限制: $FD_LIMIT${NC}"
ulimit -n 65536 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}已将 fd 限制提升到: $(ulimit -n)${NC}"
else
    # 尝试次优值
    ulimit -n 4096 2>/dev/null && \
        echo -e "${YELLOW}已将 fd 限制提升到: $(ulimit -n)${NC}" || \
        echo -e "${RED}⚠ 无法提升 fd 限制，2000+ 并发测试可能失败${NC}"
fi
echo ""

declare -A PORTS
PORTS[select]=8888
PORTS[poll]=8889
PORTS[epoll]=8890

# 并发级别数组
CONCURRENCY_LEVELS=(10 50 100 200 500 1000 2000 5000)

# 创建结果目录（在 data/ 下）
mkdir -p "$DATA_DIR"
RESULTS_DIR="$DATA_DIR/benchmark_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo -e "结果将保存到: ${GREEN}$RESULTS_DIR${NC}"
echo ""

# 遍历每个并发级别
for concurrency in "${CONCURRENCY_LEVELS[@]}"; do
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}测试并发级别: $concurrency 个客户端${NC}"
    echo -e "${YELLOW}========================================${NC}"

    for model in select poll epoll; do
        port=${PORTS[$model]}

        echo -e "\n${GREEN}测试 $model (端口 $port):${NC}"

        output=$(timeout $TIMEOUT_SEC \
            redis-benchmark -h $HOST -p $port -c $concurrency -n $TOTAL_REQUESTS -t ping -q 2>&1)

        exit_code=$?

        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}⏱ 超时 (${TIMEOUT_SEC}s)${NC}"
            echo "$concurrency,0" >> "$RESULTS_DIR/${model}.csv"
        elif [ $exit_code -ne 0 ]; then
            echo -e "  ${RED}❌ 测试失败 (exit=$exit_code)${NC}"
            echo "$concurrency,0" >> "$RESULTS_DIR/${model}.csv"
        else
            rps=$(echo "$output" | grep "PING_INLINE" | grep -oP '[\d.]+(?=\s+requests\s+per\s+second)' | head -1)

            if [ -n "$rps" ]; then
                echo -e "  ${GREEN}PING_INLINE RPS: $rps${NC}"
                echo "$concurrency,$rps" >> "$RESULTS_DIR/${model}.csv"
            else
                rps=$(echo "$output" | grep -oP '[\d.]+(?=\s+requests\s+per\s+second)' | head -1)
                if [ -n "$rps" ]; then
                    echo -e "  ${YELLOW}RPS(推定): $rps${NC}"
                    echo "$concurrency,$rps" >> "$RESULTS_DIR/${model}.csv"
                else
                    echo -e "  ${RED}⚠ 无法解析 RPS，原始输出:${NC}"
                    echo "$output" | head -3
                    echo "$concurrency,0" >> "$RESULTS_DIR/${model}.csv"
                fi
            fi
        fi

        sleep 1
    done

    echo ""
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}测试完成！${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "结果文件:"
ls -lh "$RESULTS_DIR"/*.csv

echo ""
echo "各模型数据摘要:"
for model in select poll epoll; do
    if [ -f "$RESULTS_DIR/${model}.csv" ]; then
        echo -e "\n${GREEN}$model:${NC}"
        cat "$RESULTS_DIR/${model}.csv" | while IFS=',' read -r conc rps; do
            if [ "$rps" = "0" ]; then
                printf "  并发 %4d: %s\n" "$conc" "❌ 失败"
            else
                printf "  并发 %4d: %.0f req/s\n" "$conc" "$rps"
            fi
        done
    fi
done

echo ""
echo -e "${YELLOW}运行 Python 脚本绘制图表:${NC}"
echo "  python3 $SCRIPT_DIR/ioMultiPlexing_Plot.py $RESULTS_DIR"
echo ""
echo -e "${YELLOW}快速绘图（自动在 data/ 下查找）:${NC}"
echo "  python3 $SCRIPT_DIR/ioMultiPlexing_Plot.py benchmark_results_$(date +%Y%m%d)*"
