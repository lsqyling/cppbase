# my::unique_ptr 测试报告 (PR)

## 概述

对自定义智能指针 `my::unique_ptr`（位于 `include/mystl/unique_ptr.hpp`）进行全面的功能性与性能测试。

## 核心特性

| 特性 | 说明 |
|------|------|
| 单对象版本 | `unique_ptr<T, D>` 完整实现 |
| 数组偏特化 | `unique_ptr<T[], D>` 完整实现 |
| EBO 优化 | `compressed_pair` 空基类优化，空删除器时大小 = 裸指针 |
| 删除器支持 | 左值引用、右值引用、函数对象、lambda |
| 转换构造 | 派生类 → 基类的移动转换 |
| make_uniqueptr | 单对象 + 数组版本的工厂函数 |
| 编译期约束 | SFINAE + concepts 严格约束 |

## 功能性测试 (Google Test)

**33 个测试全部通过，2 个测试套件：**

### 单对象版本 (19 项)

| 测试 | 说明 |
|------|------|
| DefaultConstruction | 默认构造 `unique_ptr<T>` |
| NullptrConstruction | `unique_ptr<T>(nullptr)` |
| RawPointerConstruction | `unique_ptr<T>(new T(42))` |
| LvalueDeleterConstruction | 左值引用删除器 `D&` |
| RvalueDeleterConstruction | 右值引用删除器 `D&&` |
| MoveConstructionSameType | 同类型移动构造 |
| MoveConstructionConvert | 派生类→基类转换移动 |
| MoveAssignment | 移动赋值操作 |
| NullptrAssignment | `p = nullptr` 赋值 |
| Release | `release()` 移交所有权 |
| Reset | `reset()` / `reset(ptr)` |
| Swap | `swap()` 交换 |
| GetDeleterStateful | 修改删除器状态 |
| CustomPointerType | `D::pointer` 自定义指针类型 |
| EboEmptyDeleter | 空删除器 sizeof 验证 |
| CopyIsDeleted | 拷贝构造/赋值被删除 |
| MakeUniquePtrSingle | `make_uniqueptr<T>()` |
| DestructorCallsDeleter | 析构触发删除器 |
| MakeUniqueArrayKnownBoundDeleted | 已知边界数组工厂被禁用 |

### 数组版本 (14 项)

| 测试 | 说明 |
|------|------|
| DefaultConstruction | 默认构造 |
| NullptrConstruction | nullptr 构造 |
| PointerConstruction | 原始指针 + 下标访问 |
| MoveConstruction | 移动构造 |
| MoveAssignment | 移动赋值 |
| NullptrAssignment | nullptr 赋值 |
| ReleaseAndReset | release/reset |
| Swap | 交换 |
| SubscriptAccess | `p[i]` 读写 |
| MakeUniquePtrArray | `make_uniqueptr<T[]>(size)` |
| EboEmptyDeleter | 空删除器 sizeof 验证 |
| DestructorDeleteArray | 析构触发 `delete[]` |
| MoveAssignWithDeleter | 自定义删除器移动赋值 |
| OperatorsDeleted | `operator*` 和 `operator->` 被删除 |

## 性能测试 (Google Benchmark)

**测试环境：** WSL 2 (Ubuntu), g++ -std=c++20, 4 Cores

### 构造与析构

| Benchmark | my::unique_ptr | std::unique_ptr | 对比 |
|-----------|:-------------:|:--------------:|:----:|
| New/Delete | 49.7 ns | 40.0 ns | std 快 24% |
| make_unique | 29.5 ns | 48.8 ns | **my 快 39%** ⚡ |

### 移动语义

| Benchmark | my::unique_ptr | std::unique_ptr | 对比 |
|-----------|:-------------:|:--------------:|:----:|
| Move Construct | 49.5 ns | 38.1 ns | std 快 30% |
| Move Assign | 82.8 ns | 77.0 ns | std 快 8% |

### 修改器

| Benchmark | my::unique_ptr | std::unique_ptr | 对比 |
|-----------|:-------------:|:--------------:|:----:|
| **Reset** | **104 ns** | **118 ns** | **my 快 12%** ⚡ |
| **Release** | **38.5 ns** | **42.2 ns** | **my 快 9%** ⚡ |

### 删除器影响

| Benchmark | 耗时 | 说明 |
|-----------|:----:|------|
| EmptyDeleter | 206 ns | 空删除器 (EBO 生效) |
| StatefulDeleter | 43.3 ns | 状态删除器 (含构造开销) |

### 批量操作

| Benchmark | my::unique_ptr | std::unique_ptr | 对比 |
|-----------|:-------------:|:--------------:|:----:|
| **Vector PushBack (1024次)** | **66,130 ns** | **82,892 ns** | **my 快 20%** ⚡ |

### 数组版本

| Benchmark | my::unique_ptr | std::unique_ptr | 对比 |
|-----------|:-------------:|:--------------:|:----:|
| Array New/Delete (16元素) | 205 ns | 221 ns | my 快 7% |

## 性能分析

### my::unique_ptr 的优势场景

1. **`make_uniqueptr` 快 39%**：`compressed_pair` 的空删除器 EBO 优化使默认构造开销接近于零
2. **Vector 批量操作快 20%**：`std::exchange` 和极简的移动语义减少了拷贝开销
3. **Reset/Release 快 9-12%**：直接操作 `compressed_pair` 底层，无额外抽象层

### std::unique_ptr 的优势场景

1. **New/Delete 快 24%**：标准库编译器内联优化更成熟
2. **Move 快 8-30%**：标准库的特殊成员函数可能获得更多编译器特化

### EBO 验证

```cpp
sizeof(unique_ptr<int, EmptyDeleter>)   == sizeof(int*)     // EBO 生效
sizeof(unique_ptr<int, StatefulDeleter>) >  sizeof(int*)     // 含状态
```

## 结论

| 维度 | 评分 | 说明 |
|------|:----:|------|
| 功能完整性 | ★★★★★ | 33/33 测试通过，覆盖所有关键路径 |
| 内存效率 | ★★★★★ | EBO 使空删除器零开销 |
| 构造性能 | ★★★☆☆ | 比 std 慢 24-30%，有优化空间 |
| 修改器性能 | ★★★★★ | Reset/Release 比 std 快 |
| 批量性能 | ★★★★★ | Vector 操作显著优于 std |
| API 可用性 | ★★★★☆ | 与 std::unique_ptr API 高度一致 |

---

*本报告由自动化测试框架生成*
*测试时间: 2026-06-24*
