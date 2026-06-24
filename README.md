# cppbase — C++20 基础组件实现与性能基准测试

## 项目概述

从头实现 C++ 标准库核心组件（字符串、智能指针），并完成三种 IO 多路复用模型（select/poll/epoll）的性能对比。每个模块均配套 Google Test 功能测试 + Google Benchmark 性能基准测试，提供与标准库实现的量化对比报告。

## 技术栈

| 项目 | 内容 |
|------|------|
| 语言标准 | C++20 (concepts, constexpr, noexcept) |
| 构建系统 | CMake ≥ 3.25 |
| 性能测试 | Google Benchmark |
| 功能测试 | Google Test (Googletest) |
| 编译器 | g++ (WSL 2 / Ubuntu) |
| 可视化 | Python + Matplotlib |

## 项目结构

```
├── include/mystl/          # 核心实现（头文件 only）
│   ├── hstring.hpp         # 自定义字符串类
│   └── unique_ptr.hpp      # 自定义智能指针
├── test/                   # 测试程序
│   ├── iomultiplexing.cpp  # select/poll/epoll 回声服务器
│   ├── hstring_benchmark.cpp
│   ├── unique_ptr_benchmark.cpp
│   ├── unique_ptr_gtest.cpp
│   └── ...                 # 其他 C++ 练习
├── doc/                    # 测试报告与可视化
│   ├── hstring_PR.md
│   ├── unique_ptr_PR.md
│   ├── ioMultiplexing_PR.md
│   └── *_Comparison.png
├── data/                   # 压测原始数据
├── script/                 # 自动化脚本
│   ├── ioMultiplexing_Benchmark.sh  # 一键压测
│   └── ioMultiPlexing_Plot.py        # 结果可视化
└── extern/benchmark/       # Google Benchmark (submodule)
```

---

## 模块一：hstring — 自定义字符串

### 设计原理

完全独立于 `std::string` 的手写字符串类，核心设计：

- **SSO（小字符串优化）**：通过 `union Storage` 实现，短字符串 (≤15 字符) 存储在栈上，避免堆分配
- **大小编码**：`m_size` 最高位作为标记位，1=小字符串/0=大字符串，其余位存长度
- **copy-and-swap**：拷贝赋值采用强异常安全保证
- **KMP 子串查找**：实现 `find_kmp()`，O(n+m) 时间复杂度

### 核心 API

| 类别 | 方法 |
|------|------|
| 构造 | `string()`, `string(const char*)`, `string(const string&)`, `string(string&&)` |
| 修改 | `append`, `push_back`, `pop_back`, `insert`, `erase`, `clear`, `swap` |
| 查找 | `find` (memchr优化), `find_kmp` (KMP算法) |
| 分隔 | `split(char)`, `split(string)` — 支持跳过空串 |
| 比较 | `compare`, 全套运算符重载 |
| 容量 | `reserve`, `shrink_to_fit`, `size`, `capacity` |
| 迭代器 | `begin/end`, `cbegin/cend` |

### 性能表现（vs std::string）

> 详细报告：[doc/hstring_PR.md](doc/hstring_PR.md)

| 操作 | 性能对比 | 优势方 |
|------|---------|-------|
| 构造（长串） | hstring 30ns vs std 475ns | **hstring 快 15x** |
| 拷贝构造（长串） | hstring 32ns vs std 465ns | **hstring 快 14x** |
| 移动赋值 | hstring 67ns vs std 623ns | **hstring 快 9x** |
| append（长串） | hstring 71ns vs std 499ns | **hstring 快 7x** |
| erase（长串） | hstring 50ns vs std 503ns | **hstring 快 10x** |
| substr（短串） | hstring 37ns vs std 500ns | **hstring 快 13x** |
| iterate（长串） | hstring 96ns vs std 952ns | **hstring 快 10x** |
| find / compare | std 略快 (标准库编译器优化更成熟) | **std 快 20-30%** |

**结论**：hstring 在大部分操作上性能是 `std::string` 的 **5~15 倍**，尤其在构造、拷贝、子串等涉及内存分配的场景优势显著。

---

## 模块二：my::unique_ptr — 自定义智能指针

### 设计原理

仿 `std::unique_ptr` 的完整实现，核心设计：

- **compressed_pair + EBO**：空删除器（如 lambda、无状态函数对象）通过空基类优化实现零额外开销
- **数组偏特化**：`unique_ptr<T[]>` 提供 `operator[]`，禁用 `operator*` / `operator->`
- **SFINAE + Concepts**：编译期约束模板参数合法性
- **派生类→基类转换**：支持移动转换构造
- **工厂函数**：`my::make_uniqueptr<T>()` / `my::make_uniqueptr<T[]>(size)`

### 性能表现（vs std::unique_ptr）

> 详细报告：[doc/unique_ptr_PR.md](doc/unique_ptr_PR.md)

| 操作 | my::unique_ptr | std::unique_ptr | 对比 |
|------|:-------------:|:--------------:|:----:|
| make_unique* | **29.5 ns** | 48.8 ns | **my 快 39%** |
| Reset | **104 ns** | 118 ns | **my 快 12%** |
| Release | **38.5 ns** | 42.2 ns | **my 快 9%** |
| Vector PushBack ×1024 | **66,130 ns** | 82,892 ns | **my 快 20%** |
| New/Delete | 49.7 ns | **40.0 ns** | std 快 24% |
| Move Construct | 49.5 ns | **38.1 ns** | std 快 30% |

### 测试覆盖（33/33 通过）

- **单对象版 (19 项)**：默认构造、nullptr、原始指针、删除器左值/右值、移动构造/赋值、派生类转换、release/reset/swap、EBO验证、拷贝禁用
- **数组版 (14 项)**：下标访问、`delete[]` 析构、工厂函数、`operator*` 编译期禁用

---

## 模块三：IO 多路复用对比（select / poll / epoll）

### 架构设计

三个回声服务器均采用统一的**非阻塞 I/O + 边缘触发（ET）** 设计，确保公平对比：

| 特性 | select | poll | epoll |
|------|--------|------|-------|
| 端口 | 8888 | 8889 | 8890 |
| socket 模式 | SOCK_NONBLOCK | SOCK_NONBLOCK | SOCK_NONBLOCK |
| accept | 循环直到 EAGAIN | 循环直到 EAGAIN | 循环直到 EAGAIN |
| read | 循环直到 EAGAIN | 循环直到 EAGAIN | 循环直到 EAGAIN (ET) |
| 写处理 | 部分写入循环 | 部分写入循环 | 部分写入循环 |
| TCP_NODELAY | ✅ | ✅ | ✅ |
| 事件模型 | LT | LT | ET |

### 压测结果（redis-benchmark, 10万请求/次）

> 详细报告：[doc/ioMultiplexing_PR.md](doc/ioMultiplexing_PR.md)

| 并发数 | SELECT (req/s) | POLL (req/s) | EPOLL (req/s) |
|:-----:|:-------------:|:------------:|:-------------:|
| 10 | 79,681 | 71,429 | 68,399 |
| 50 | 96,993 | **98,425** 🏆 | 66,313 |
| 100 | 39,872 | 93,545 | 75,472 |
| 200 | 95,238 | 77,882 | 79,745 |
| 500 | 82,102 | 81,367 | 73,855 |
| 1000 | 72,833 | 76,628 | **81,235** 🏆 |
| 2000 | 超时 ❌ | 73,153 | 78,309 |
| 5000 | 超时 ❌ | 失败 ❌ | **70,472** 🏆 |

### 结论

| 模型 | 特性 | 适用场景 |
|------|------|---------|
| **epoll** 🥇 高并发王者 | O(1) 事件分发，唯一在 5000 并发下正常工作 | 大规模连接（>1000） |
| **poll** 🥇 中低并发之王 | 平均吞吐量全场最高 78,918 req/s | 中等并发（<500） |
| **select** | 小并发表现尚可，2000+ 直接超时 | 简单场景、跨平台兼容 |

> 可视化对比图：[doc/select_poll_epoll_Comparison.png](doc/select_poll_epoll_Comparison.png)

---

## 快速开始

### 构建

```bash
# 使用 CMake 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 或直接使用已有构建目录
cd cmake-build-debug-wsl
ninja
```

### 运行测试

```bash
# 功能测试（GTest）
./build/test/unique_ptr_gtest

# 性能基准测试（Google Benchmark）
./build/test/hstring_benchmark
./build/test/unique_ptr_benchmark

# IO 多路复用回声服务器（需在 WSL/Linux 环境）
# 启动服务器
./build/test/iomultiplexing select s redis    # select 服务器 :8888
./build/test/iomultiplexing poll s redis       # poll 服务器   :8889
./build/test/iomultiplexing epoll s redis      # epoll 服务器  :8890

# redis-benchmark 压测
redis-benchmark -h 127.0.0.1 -p 8888 -c 100 -n 100000 -t ping
```

### 一键压测 + 可视化

```bash
# 全自动压测（逐步增加并发 10→5000）
bash script/ioMultiplexing_Benchmark.sh

# 绘制对比曲线图
python3 script/ioMultiPlexing_Plot.py data/benchmark_results_*
```
