#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IO Multiplexing Performance Plotter
根据 benchmark 结果绘制 select/poll/epoll 性能对比图表
用法: python3 ioMultiPlexing_Plot.py <results_directory>
      python3 ioMultiPlexing_Plot.py benchmark_results_YYYYMMDD_HHMMSS

数据目录规则：
  1. 直接作为路径查找
  2. 若未找到，自动在 data/ 下查找
"""

import sys
import os

# 脚本所在目录的上级作为项目根目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(PROJECT_ROOT, 'data')
import matplotlib.pyplot as plt
import matplotlib
import numpy as np

# 设置字体：WSL 可能没有 SimHei，用 DejaVu Sans + 'sans-serif' fallback
matplotlib.rcParams['font.family'] = ['DejaVu Sans', 'sans-serif']
matplotlib.rcParams['axes.unicode_minus'] = False


def load_csv(filepath):
    """
    加载 CSV 文件。
    格式: 每行 "concurrency,rps"
    如果同一并发级别有多行，取最后一行的值（覆盖前面的重复）。
    返回: (并发表, RPS表) - 按并发数升序排列
    """
    data = {}
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            if len(parts) == 2:
                try:
                    concurrency = int(parts[0])
                    rps = float(parts[1])
                    data[concurrency] = rps
                except ValueError:
                    continue

    sorted_items = sorted(data.items())
    if not sorted_items:
        return [], []

    concurrencies, rps_values = zip(*sorted_items)
    return list(concurrencies), list(rps_values)


def plot_results(results_dir):
    """绘制性能对比图"""
    colors = {
        'select': '#E74C3C',
        'poll':   '#2ECC71',
        'epoll':  '#3498DB'
    }
    markers = {
        'select': 'o',
        'poll':   's',
        'epoll':  '^'
    }
    line_styles = {
        'select': '-',
        'poll':   '--',
        'epoll':  '-.'
    }

    models_order = ['select', 'poll', 'epoll']

    print(f"\n📂 加载测试结果 from: {results_dir}")
    print("=" * 60)

    all_data = {}
    all_concurrencies = set()

    for model in models_order:
        csv_file = os.path.join(results_dir, f"{model}.csv")
        if not os.path.exists(csv_file):
            print(f"  ⚠️  找不到 {csv_file}，跳过 {model}")
            continue

        concurrencies, rps_values = load_csv(csv_file)

        if not concurrencies:
            print(f"  ⚠️  {csv_file} 无有效数据，跳过 {model}")
            continue

        valid_data = [(c, r) for c, r in zip(concurrencies, rps_values) if r > 0]
        if not valid_data:
            print(f"  ⚠️  {model}: 所有数据点均为 0（全部失败）")
        else:
            valid_conc, valid_rps = zip(*valid_data)
            all_data[model] = {
                'concurrencies': list(valid_conc),
                'rps': list(valid_rps),
                'total_points': len(concurrencies),
                'failed_points': sum(1 for r in rps_values if r == 0)
            }
            all_concurrencies.update(valid_conc)

            fail_info = f" (失败 {all_data[model]['failed_points']} 个点)" if all_data[model]['failed_points'] > 0 else ""
            print(f"  ✅ {model.upper()}: {len(valid_conc)} 个有效数据点{fail_info}")
            print(f"     并发范围: {min(valid_conc)} - {max(valid_conc)}")
            print(f"     RPS 范围: {min(valid_rps):.2f} - {max(valid_rps):.2f}")

    if not all_data:
        print("  ❌ 没有有效数据，退出")
        return

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 7))

    # === 左图: 吞吐量曲线 ===
    for model in models_order:
        if model not in all_data:
            continue
        d = all_data[model]
        ax1.plot(
            d['concurrencies'], d['rps'],
            marker=markers[model],
            linestyle=line_styles[model],
            linewidth=2.5,
            markersize=10,
            label=model.upper(),
            color=colors[model],
            alpha=0.85,
            markeredgecolor='white',
            markeredgewidth=1.5
        )

        max_rps = max(d['rps'])
        max_idx = d['rps'].index(max_rps)
        max_conc = d['concurrencies'][max_idx]

        if max_idx == len(d['rps']) - 1 and len(d['rps']) > 1:
            xytext = (max_conc * 0.6, max_rps * 1.05)
        else:
            xytext = (max_conc * 1.3, max_rps * 0.92)

        ax1.annotate(
            f'{max_rps:.0f}',
            xy=(max_conc, max_rps),
            xytext=xytext,
            fontsize=10,
            fontweight='bold',
            color=colors[model],
            arrowprops=dict(arrowstyle='->', color=colors[model], lw=1.5),
            bbox=dict(boxstyle='round,pad=0.2', facecolor='white', edgecolor=colors[model], alpha=0.7)
        )

    ax1.set_xlabel('Number of Concurrent Clients', fontsize=13, fontweight='bold')
    ax1.set_ylabel('Requests / Second', fontsize=13, fontweight='bold')
    ax1.set_title('IO Multiplexing Throughput Comparison\n(Select vs Poll vs Epoll)',
                  fontsize=15, fontweight='bold')
    ax1.legend(loc='upper right', fontsize=12, framealpha=0.85)
    ax1.grid(True, alpha=0.3, linestyle='--', linewidth=0.6)
    ax1.set_xscale('log', base=10)
    ax1.tick_params(labelsize=11)
    ax1.set_xlim(left=8)

    # === 右图: 理论分析 + 总结 ===
    ax2.axis('off')

    lines = []
    lines.append("=" * 35)
    lines.append("Performance Analysis Summary")
    lines.append("=" * 35)
    lines.append("")

    for model in models_order:
        if model not in all_data:
            continue
        d = all_data[model]
        max_rps = max(d['rps'])
        avg_rps = np.mean(d['rps'])
        peak_conc = d['concurrencies'][d['rps'].index(max_rps)]

        lines.append(f"  {model.upper()}:")
        lines.append(f"    Peak: {max_rps:>8,.0f} req/s @ {peak_conc} conn")
        lines.append(f"    Avg:  {avg_rps:>8,.0f} req/s")
        if d['failed_points'] > 0:
            lines.append(f"    Fail: {d['failed_points']} tests")
        lines.append("")

    best_models = {}
    for model in models_order:
        if model in all_data:
            best_models[model] = np.mean(all_data[model]['rps'])

    if best_models:
        best_model = max(best_models, key=best_models.get)
        second_best = sorted(best_models.items(), key=lambda x: -x[1])

        lines.append("-" * 35)
        lines.append(f"  [[Best Avg]] {best_model.upper()}")
        if len(second_best) >= 2:
            improvement = ((second_best[0][1] / second_best[1][1]) - 1) * 100
            lines.append(f"  比 {second_best[1][0].upper()} 提升 {improvement:.1f}%")
        lines.append("")

        high_conc_data = {}
        for model, d in all_data.items():
            for c, r in zip(d['concurrencies'], d['rps']):
                if c >= 1000:
                    if model not in high_conc_data:
                        high_conc_data[model] = []
                    high_conc_data[model].append(r)

        if high_conc_data:
            lines.append("📊 高并发 (>=1000):")
            for model in models_order:
                if model in high_conc_data and high_conc_data[model]:
                    avg = np.mean(high_conc_data[model])
                    lines.append(f"  {model.upper()}: {avg:,.0f} req/s")

    lines.append("")
    lines.append("=" * 35)
    lines.append("Conclusion")
    lines.append("=" * 35)
    lines.append("")
    lines.append("• select: O(n) scan, FD_SETSIZE limited")
    lines.append("  → Low concurrency: comparable performance")
    lines.append("  → High concurrency: limited by fd_set")
    lines.append("")
    lines.append("• poll:   O(n) scan, no fd limit")
    lines.append("  → Array-based, simpler than epoll")
    lines.append("  → Performance degrades linearly")
    lines.append("")
    lines.append("• epoll:  O(1) event-driven, callback-based")
    lines.append("  → Edge-triggered + Non-blocking I/O")
    lines.append("  → Excels at high concurrency")
    lines.append("  → Consistent throughput under load")

    props = dict(boxstyle='round,pad=0.5', facecolor='#F9F9F9',
                 edgecolor='#CCCCCC', alpha=0.95)
    ax2.text(0.05, 0.96, "\n".join(lines),
             transform=ax2.transAxes,
             fontsize=10, verticalalignment='top',
             bbox=props, family='DejaVu Sans Mono',
             linespacing=1.4)

    plt.tight_layout()

    output_file = os.path.join(PROJECT_ROOT, 'doc/select_poll_epoll_Comparison.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n📈 性能对比图已保存到: {output_file}")

    plt.show()

    print_detailed_conclusion(all_data)


def print_detailed_conclusion(all_data):
    """打印详细的性能分析结论"""
    print("\n" + "=" * 70)
    print("                   性能对比详细分析")
    print("=" * 70)

    models_order = ['select', 'poll', 'epoll']

    print(f"\n{'并发数':>8}", end='')
    for m in models_order:
        print(f"  {m.upper():>10}", end='')
    print()

    all_conc = sorted(set(
        c for d in all_data.values() for c in d['concurrencies']
    ))

    for conc in all_conc:
        print(f"{conc:>8}", end='')
        for m in models_order:
            if m in all_data and conc in all_data[m]['concurrencies']:
                idx = all_data[m]['concurrencies'].index(conc)
                rps = all_data[m]['rps'][idx]
                if rps > 0:
                    print(f"  {rps:>10,.0f}", end='')
                else:
                    print(f"  {'FAILED':>10}", end='')
            else:
                print(f"  {'N/A':>10}", end='')
        print()

    print("\n" + "-" * 70)
    print("结论:")
    print("-" * 70)

    for model in models_order:
        if model not in all_data:
            continue
        d = all_data[model]
        max_rps = max(d['rps'])
        max_conc = d['concurrencies'][d['rps'].index(max_rps)]

        high = [(c, r) for c, r in zip(d['concurrencies'], d['rps'])
                if c >= 1000 and r > 0]
        if high:
            high_rps = np.mean([r for _, r in high])
            print(f"  • {model.upper()}: 峰值 {max_rps:.0f} req/s (并发 {max_conc})")
            print(f"    高并发(>=1000)平均: {high_rps:.0f} req/s")
        else:
            print(f"  • {model.upper()}: 峰值 {max_rps:.0f} req/s (并发 {max_conc})")

    print()
    if all(m in all_data for m in models_order):
        select_avg = np.mean(all_data['select']['rps'])
        poll_avg = np.mean(all_data['poll']['rps'])
        epoll_avg = np.mean(all_data['epoll']['rps'])

        common_conc = set(all_data['select']['concurrencies'])
        for m in models_order:
            common_conc &= set(all_data[m]['concurrencies'])

        common_conc = sorted(common_conc)
        if common_conc:
            print(f"  共同有效并发级别: {common_conc}")
            select_common = [all_data['select']['rps'][all_data['select']['concurrencies'].index(c)]
                           for c in common_conc]
            poll_common = [all_data['poll']['rps'][all_data['poll']['concurrencies'].index(c)]
                         for c in common_conc]
            epoll_common = [all_data['epoll']['rps'][all_data['epoll']['concurrencies'].index(c)]
                          for c in common_conc]

            valid_triples = [(s, p, e) for s, p, e in zip(select_common, poll_common, epoll_common)
                           if s > 0 and p > 0 and e > 0]
            if valid_triples:
                select_avg2 = np.mean([s for s, _, _ in valid_triples])
                poll_avg2 = np.mean([p for _, p, _ in valid_triples])
                epoll_avg2 = np.mean([e for _, _, e in valid_triples])

                print()
                print(f"  同条件对比 (相同并发级别):")
                print(f"    SELECT vs POLL:  {select_avg2:,.0f} vs {poll_avg2:,.0f}")
                print(f"    SELECT vs EPOLL: {select_avg2:,.0f} vs {epoll_avg2:,.0f}")
                print(f"    POLL  vs EPOLL:  {poll_avg2:,.0f} vs {epoll_avg2:,.0f}")

    print("\n" + "=" * 70)


def main():
    if len(sys.argv) < 2:
        print("用法: python3 ioMultiPlexing_Plot.py <results_directory>")
        print("示例: python3 ioMultiPlexing_Plot.py benchmark_results_20260624_100135")
        sys.exit(1)

    results_dir = sys.argv[1]

    # 先尝试直接作为路径
    if os.path.isdir(results_dir):
        plot_results(results_dir)
        return

    # 再尝试 data/ 下查找
    data_path = os.path.join(DATA_DIR, results_dir)
    if os.path.isdir(data_path):
        print(f"📁 在 data/ 下找到数据目录: {data_path}")
        plot_results(data_path)
        return

    print(f"错误: 目录 '{results_dir}' 不存在（已尝试直接路径和 data/ 下）")
    sys.exit(1)


if __name__ == "__main__":
    main()
