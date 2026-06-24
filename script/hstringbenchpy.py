#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import subprocess
import sys
import matplotlib.pyplot as plt
from collections import defaultdict

# -------------------- 配置 --------------------
BENCHMARK_EXE = "../cmake-build-release/test/test_hstring_benchmark.exe"   # 可执行文件路径
OUTPUT_IMAGE_PREFIX = "benchmark_"                # 输出图片前缀
# ----------------------------------------------

def run_benchmark(executable):
    """运行 benchmark 可执行文件，返回标准输出字符串"""
    try:
        result = subprocess.run([executable], capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"运行 benchmark 失败: {e}")
        print(e.stderr)
        sys.exit(1)

def parse_time_value(time_str):
    """将 '123 ns' 转换为浮点数（单位：纳秒）"""
    return float(time_str.split()[0])

def parse_output(output):
    """
    解析 benchmark 输出，返回数据结构：
    data[operation][type][length] = time_ns
    其中 type 为 'std', 'hstring', 'extra' 之一
    length 为 'short', 'long' 或 None（用于无长度区分的 extra 操作）
    """
    lines = output.splitlines()
    benchmark_pattern = re.compile(
        r'^(?P<name>[\w/]+)\s+(?P<time>\d+\.?\d* ns)\s+(?P<cpu>\d+\.?\d* ns)\s+(?P<iter>\d+)$'
    )
    # 前缀识别
    prefix_map = {
        'std_string_': 'std',
        'hstring_': 'hstring',
        'BM_MyString': 'extra',
    }
    # 剩余部分解析：操作名，可能带有 _short/_long 和 /数字
    op_len_pattern = re.compile(r'^(?P<op>.*?)(_(?P<len>short|long))?(/\d+)?$')

    raw_times = defaultdict(list)   # key: (type, op, length) -> list of times

    for line in lines:
        m = benchmark_pattern.match(line)
        if not m:
            continue
        name = m.group('name')
        time_ns = parse_time_value(m.group('time'))

        # 识别前缀
        matched_prefix = None
        for prefix, typ in prefix_map.items():
            if name.startswith(prefix):
                matched_prefix = prefix
                type_ = typ
                rest = name[len(prefix):]
                break
        if matched_prefix is None:
            continue

        # 解析剩余部分
        m2 = op_len_pattern.match(rest)
        if not m2:
            continue
        op = m2.group('op')
        length = m2.group('len')   # 可能为 None
        # 对于 extra 类型，可能没有长度字段，直接使用 op 作为操作名
        if type_ == 'extra':
            op = rest   # 保持原名，便于区分不同 extra 操作
            length = None

        key = (type_, op, length)
        raw_times[key].append(time_ns)

    # 计算平均值
    data = defaultdict(lambda: defaultdict(dict))   # data[op][type][length] = avg_time
    for (type_, op, length), times in raw_times.items():
        avg_time = sum(times) / len(times)
        data[op][type_][length] = avg_time

    return data

def plot_comparisons(data):
    """根据解析数据生成图表"""
    # 分离有对比的操作（同时存在 std 和 hstring）和仅 hstring 的操作
    comparison_ops = []
    extra_ops = []   # 仅 hstring 且无长度区分的操作

    for op, types in data.items():
        has_std = 'std' in types
        has_hstring = 'hstring' in types
        # 如果有 std 和 hstring，且两者都包含 short/long 长度（至少有一种长度存在）
        if has_std and has_hstring:
            std_lengths = set(types['std'].keys())
            hstring_lengths = set(types['hstring'].keys())
            if std_lengths or hstring_lengths:
                comparison_ops.append(op)
        # 如果只有 hstring，且可能带有长度或无长度
        elif has_hstring and not has_std:
            if any(types['hstring'].keys()):   # 有长度数据，可以画线
                comparison_ops.append(op)       # 但只画 hstring 线
            else:
                extra_ops.append(op)
        # 其他情况（只有 std）可忽略或也加入对比图

    comparison_ops = sorted(set(comparison_ops))
    extra_ops = sorted(set(extra_ops))

    # -------------------- 绘制对比图（折线图，每个子图包含 short 和 long） --------------------
    if comparison_ops:
        n = len(comparison_ops)
        cols = min(3, n)
        rows = (n + cols - 1) // cols
        fig, axes = plt.subplots(rows, cols, figsize=(5*cols, 4*rows))
        if rows == 1 and cols == 1:
            axes = [axes]
        else:
            axes = axes.flatten()
        fig.suptitle('Performance Comparison: std::string vs hstring', fontsize=16, y=1.05)

        for idx, op in enumerate(comparison_ops):
            ax = axes[idx]
            types_dict = data[op]

            # x 轴标签：short 和 long
            lengths = ['short', 'long']
            x = range(len(lengths))

            # 绘制 std 线（如果存在）
            if 'std' in types_dict:
                std_times = [types_dict['std'].get(l, None) for l in lengths]
                line_std, = ax.plot(x, std_times, marker='o', linestyle='-', linewidth=2, label='std::string', color='C0')
                # 添加数据标签
                for i, val in enumerate(std_times):
                    if val is not None:
                        ax.annotate(f'{val:.0f}', (x[i], val), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8)

            # 绘制 hstring 线（如果存在）
            if 'hstring' in types_dict:
                hstring_times = [types_dict['hstring'].get(l, None) for l in lengths]
                line_hstring, = ax.plot(x, hstring_times, marker='s', linestyle='--', linewidth=2, label='hstring', color='C1')
                for i, val in enumerate(hstring_times):
                    if val is not None:
                        ax.annotate(f'{val:.0f}', (x[i], val), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8)

            ax.set_xticks(x)
            ax.set_xticklabels(lengths)
            ax.set_ylabel('Time (ns)')
            ax.set_title(op)
            ax.legend(loc='upper left')
            ax.grid(True, linestyle='--', alpha=0.6)

        # 隐藏多余的子图
        for idx in range(len(comparison_ops), len(axes)):
            axes[idx].set_visible(False)

        plt.tight_layout()
        plt.savefig(f'{OUTPUT_IMAGE_PREFIX}comparison.png', dpi=150)
        plt.close()
        print(f"对比图已保存为 {OUTPUT_IMAGE_PREFIX}comparison.png")

    # -------------------- 绘制 extra 操作柱状图 --------------------
    if extra_ops:
        plt.figure(figsize=(max(8, len(extra_ops)*1.2), 5))
        ops = extra_ops
        times = [data[op]['extra'][None] for op in ops]   # extra 操作没有长度，直接取值
        colors = plt.cm.viridis(range(len(ops)))
        bars = plt.bar(ops, times, color=colors)
        plt.ylabel('Time (ns)')
        plt.title('hstring Exclusive Operations')
        plt.xticks(rotation=45, ha='right')
        # 在柱子上标注数值
        for bar, t in zip(bars, times):
            plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(times)*0.01,
                     f'{t:.0f}', ha='center', va='bottom', fontsize=9)
        plt.grid(axis='y', linestyle='--', alpha=0.6)
        plt.tight_layout()
        plt.savefig(f'{OUTPUT_IMAGE_PREFIX}extra.png', dpi=150)
        plt.close()
        print(f"额外操作图已保存为 {OUTPUT_IMAGE_PREFIX}extra.png")
    if not comparison_ops and not extra_ops:
        print("未解析到任何有效 benchmark 数据。")

def main():
    print(f"正在执行 benchmark: {BENCHMARK_EXE} ...")
    output = run_benchmark(BENCHMARK_EXE)
    print("解析输出...")
    data = parse_output(output)
    if not data:
        print("未解析到任何测试项，请检查 benchmark 输出格式。")
        sys.exit(1)

    print(f"共解析到 {len(data)} 个操作。")
    plot_comparisons(data)
    print("图表生成完成。")

if __name__ == "__main__":
    main()