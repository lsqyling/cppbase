# `hstring`字符串操作类设计与实现

## 一、项目概要说明
目标：设计并实现字符串操作类hstring，实现自定义字符串的增、删、改、查等核心功能，通过缓冲区机制减少内存频繁分配／释放的开销，
全面考察类设计、运算符重载及算法实现能力。

核心特性：
* 不依赖C++标准库string类，完全自主实现底层逻辑；
* SSO优化，简化内存结构
```c++
private:
    // SSO 缓冲区大小（包括结尾的 '\0'），可存储 small_capacity 个字符（不含 null）
    static constexpr size_type small_capacity = 15;          // 例如 15 + 1 = 16 字节

    // 存储联合体：大字符串用 dynamic，小字符串用 local
    union Storage {
        struct {
            value_type * data;         // 指向动态分配的字符数组（包含结尾 '\0'）
            size_type capacity; // 分配的容量（包含结尾 '\0' 的空间）
        } dynamic;
        value_type  local[small_capacity + 1];   // 本地缓冲区，始终以 '\0' 结尾
    } m_storage;

    // 大小与标志位的编码：
    // 最高位为 1 表示小字符串，其余位存储实际长度；
    // 最高位为 0 表示大字符串，其余位存储实际长度。
    size_type m_size;   // 编码后的大小

```
* 强异常的安全保证（copy-and-swap技术）
* 迭代器与标准库std::string的兼容
* 支持运算符重载 
* 常用增删改查的api，增加算法find_kmp结构
```c++
namespace my {
class string {
public:
    // 常用类型定义
    using value_type             = char;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using iterator               = pointer;
    using const_iterator         = const_pointer;

    static constexpr size_type npos = static_cast<size_type>(-1);

    // ---------- 构造 / 析构 ----------

    // 默认构造：空字符串（小字符串）
    string() noexcept ;

    // 从 std::string 构造
    string(const std::string &s) ;

    // 从 C 字符串构造（允许 nullptr 视为空）
    string(const value_type * s) ;

    // 从 C 字符串 + 长度构造
    string(const char* s, size_type count) ;

    // 拷贝构造
    string(const string& other) ;

    // 移动构造
    string(string&& other) noexcept ;j

    // ---------- 赋值运算符 ----------

    // 拷贝赋值（使用 copy-and-swap，强异常安全）
    string& operator=(const string& other) ;

    // 移动赋值（简单交换资源，noexcept）
    string& operator=(string&& other) noexcept ;

    // 从 C 字符串赋值
    string& operator=(const char* s) ;

    // ---------- 元素访问 ----------

    reference operator[](size_type pos) noexcept ;

    const_reference operator[](size_type pos) const noexcept ;

    reference at(size_type pos) ;

    const_reference at(size_type pos) const ;

    reference front() noexcept ;

    const_reference front() const noexcept ;

    reference back() noexcept ;

    const_reference back() const noexcept ;

    const_pointer c_str() const noexcept ;

    const_pointer data() const noexcept ;

    // ---------- 迭代器 ----------

    iterator begin() noexcept ;

    const_iterator begin() const noexcept ;

    const_iterator cbegin() const noexcept ;

    iterator end() noexcept ;

    const_iterator end() const noexcept ;

    const_iterator cend() const noexcept ;

    // ---------- 容量 ----------

    size_type size() const noexcept ;

    size_type length() const noexcept ;

    size_type capacity() const noexcept ;

    bool empty() const noexcept ;

    void reserve(size_type new_cap) ;

    void shrink_to_fit() ;

    // ---------- 修改操作 ----------

    void clear() noexcept ;

    void push_back(char ch) ;

    void pop_back() ;

    string& append(const string& str) ;

    string& append(const char* s, size_type count) ;

    string& operator+=(const string& str) ;

    string& operator+=(char ch) ;

    // 插入、擦除、替换等可类似实现，此处仅给出简单 erase 示例
    string& erase(size_type pos = 0, size_type count = npos) ;
    string &insert(size_type pos, const string &str) ;

    string &insert(size_type pos, const char *s) ;

    string &insert(size_type pos, const char *s, size_type count) ;
    // 4. 在迭代器位置 p 之前插入一个字符 ch，返回指向插入字符的迭代器
    iterator insert(const_iterator p, value_type ch) ;

    // 在迭代器位置 p 之前插入 count 个相同字符 ch，返回指向第一个插入字符的迭代器
    iterator insert(const_iterator p, size_type count, value_type ch) ;

    // 交换
    void swap(string& other) noexcept ;

    // ---------- 查找 ----------
    size_type find(const string& str, size_type pos = 0) const noexcept ;


    // 使用 KMP 算法查找子串 str，从位置 pos 开始
    size_type find_kmp(const string& str, size_type pos = 0) const ;

    // 使用 KMP 算法查找子串 s（长度为 count），从位置 pos 开始
    size_type find_kmp(const_pointer s, size_type pos, size_type count) const ;

    size_type find(const char* s, size_type pos, size_type count) const noexcept ;

    // ---------- 比较 ----------
    int compare(const string& other) const noexcept ;

    // ---------- 子串 ----------
    string substr(size_type pos = 0, size_type count = npos) const ;
    // ---------- 分隔 ----------
    std::vector<string> split(char delimiter, bool skip_empty = true) ;

    std::vector<string> split(const string &delimiter, bool skip_empty = true);
    

};

// ---------- 非成员运算符重载 ----------

inline bool operator==(const string& lhs, const string& rhs) noexcept ;

inline bool operator!=(const string& lhs, const string& rhs) noexcept ;

inline bool operator<(const string& lhs, const string& rhs) noexcept ;

inline bool operator<=(const string& lhs, const string& rhs) noexcept ;

inline bool operator>(const string& lhs, const string& rhs) noexcept ;

inline bool operator>=(const string& lhs, const string& rhs) noexcept ;

// 与 C 字符串的比较
inline bool operator==(const string& lhs, const char* rhs) ;
inline bool operator==(const char* lhs, const string& rhs) ;
// ... 其他比较运算符可类似定义

inline string operator+(const string& lhs, const string& rhs) ;
inline string operator+(const string& lhs, const char* rhs) ;
inline string operator+(const char* lhs, const string& rhs) ;
inline string operator+(const string& lhs, char rhs) ;
inline string operator+(char lhs, const string& rhs) ;

// 输出流支持
inline std::ostream& operator<<(std::ostream& os, const string& str) ;
} // namespace my

using hstring = my::string;
```
## 二、性能测试与标准库`std::string`的比较
* `benchmark` 输出数据格式
```
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 4096 KiB (x1)
***WARNING*** Library was built as DEBUG. Timings may be affected.
------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------
std_string_construct_long/0          475 ns          460 ns      1493333
std_string_construct_long/1         1061 ns         1050 ns       640000
std_string_construct_long/0          474 ns          476 ns      1445161
std_string_construct_long/1         1080 ns         1074 ns       640000
hstring_construct_long/0            30.6 ns         30.7 ns     22400000
hstring_construct_long/1             309 ns          302 ns      2635294
hstring_construct_long/0            27.4 ns         26.7 ns     26352941
hstring_construct_long/1             291 ns          292 ns      2357895
std_string_copy_long/0               465 ns          465 ns      1544828
std_string_copy_long/1               978 ns          942 ns       746667
std_string_copy_long/0               503 ns          502 ns      1120000
std_string_copy_long/1              1078 ns         1046 ns       896000
hstring_copy_long/0                 33.8 ns         31.8 ns     18666667
hstring_copy_long/1                  278 ns          279 ns      2635294
hstring_copy_long/0                 28.7 ns         28.3 ns     24888889
hstring_copy_long/1                  283 ns          285 ns      2357895
std_string_move_long/0               912 ns          903 ns       640000
std_string_move_long/1              1296 ns         1311 ns       560000
std_string_move_long/0               929 ns          942 ns       746667
std_string_move_long/1              1296 ns         1311 ns       560000
hstring_move_long/0                 63.3 ns         62.8 ns      8960000
hstring_move_long/1                  315 ns          315 ns      2133333
hstring_move_long/0                 62.3 ns         62.8 ns     11200000
hstring_move_long/1                  320 ns          318 ns      2357895
std_string_copy_assign_long/0       46.7 ns         47.5 ns     15448276
std_string_copy_assign_long/1       54.0 ns         53.0 ns     11200000
std_string_copy_assign_long/0       46.8 ns         47.1 ns     14933333
std_string_copy_assign_long/1       54.5 ns         54.4 ns     11200000
hstring_copy_assign_long/0          70.9 ns         71.1 ns      7466667
hstring_copy_assign_long/1           332 ns          322 ns      2133333
hstring_copy_assign_long/0          70.9 ns         71.1 ns     11200000
hstring_copy_assign_long/1           332 ns          330 ns      2133333
std_string_move_assign_long/0        623 ns          614 ns      1120000
std_string_move_assign_long/1        996 ns         1001 ns       640000
std_string_move_assign_long/0        624 ns          628 ns      1120000
std_string_move_assign_long/1       1024 ns         1025 ns       640000
hstring_move_assign_long/0          66.9 ns         67.0 ns     11200000
hstring_move_assign_long/1           315 ns          308 ns      1723077
hstring_move_assign_long/0          66.5 ns         67.0 ns     11200000
hstring_move_assign_long/1           318 ns          318 ns      2357895
std_string_index_long/0             65.6 ns         65.6 ns     11200000
std_string_index_long/1             1229 ns         1228 ns       560000
std_string_index_long/0             65.3 ns         65.6 ns     11200000
std_string_index_long/1             1215 ns         1193 ns       497778
hstring_index_long/0                60.2 ns         61.4 ns     11200000
hstring_index_long/1                1056 ns         1050 ns       640000
hstring_index_long/0                60.7 ns         61.4 ns     11200000
hstring_index_long/1                1054 ns         1050 ns       640000
std_string_shrink_long/0            23.9 ns         24.0 ns     28000000
std_string_shrink_long/1            53.9 ns         54.4 ns     11200000
std_string_shrink_long/0            23.5 ns         23.5 ns     29866667
std_string_shrink_long/1            53.9 ns         53.0 ns     11200000
hstring_shrink_long/0               26.2 ns         26.1 ns     26352941
hstring_shrink_long/1               28.8 ns         29.2 ns     23578947
hstring_shrink_long/0               28.8 ns         28.5 ns     26352941
hstring_shrink_long/1               28.9 ns         28.9 ns     24888889
std_string_push_back_long/0         37.4 ns         37.7 ns     18666667
std_string_push_back_long/1         42.6 ns         42.6 ns     17230769
std_string_push_back_long/0         37.9 ns         38.4 ns     15448276
std_string_push_back_long/1         40.5 ns         40.8 ns     17230769
hstring_push_back_long/0            48.1 ns         48.7 ns     14451613
hstring_push_back_long/1            47.5 ns         47.4 ns     11200000
hstring_push_back_long/0            49.3 ns         49.2 ns     14933333
hstring_push_back_long/1            48.2 ns         47.6 ns     14451613
std_string_append_long/0             499 ns          500 ns      1000000
std_string_append_long/1             895 ns          900 ns       746667
std_string_append_long/0             518 ns          531 ns      1000000
std_string_append_long/1             908 ns          921 ns       746667
hstring_append_long/0               70.9 ns         71.5 ns      8960000
hstring_append_long/1                594 ns          600 ns      1120000
hstring_append_long/0               71.4 ns         71.1 ns     11200000
hstring_append_long/1                587 ns          575 ns       896000
std_string_insert_long/0             535 ns          516 ns      1120000
std_string_insert_long/1             921 ns          921 ns       746667
std_string_insert_long/0             541 ns          530 ns      1120000
std_string_insert_long/1             939 ns          942 ns       746667
hstring_insert_long/0               70.3 ns         71.1 ns     11200000
hstring_insert_long/1                616 ns          614 ns      1120000
hstring_insert_long/0               69.2 ns         69.8 ns     11200000
hstring_insert_long/1                619 ns          614 ns      1120000
std_string_erase_long/0              503 ns          502 ns      1120000
std_string_erase_long/1              912 ns          900 ns       746667
std_string_erase_long/0              506 ns          516 ns      1000000
std_string_erase_long/1              878 ns          879 ns       746667
hstring_erase_long/0                49.5 ns         50.2 ns     14933333
hstring_erase_long/1                 301 ns          305 ns      2357895
hstring_erase_long/0                47.2 ns         46.0 ns     11200000
hstring_erase_long/1                 298 ns          298 ns      2357895
std_string_find_long/0              32.7 ns         32.5 ns     23578947
std_string_find_long/1              53.5 ns         53.1 ns     10000000
std_string_find_long/0              31.0 ns         30.0 ns     22400000
std_string_find_long/1              51.2 ns         50.0 ns     10000000
hstring_find_long/0                 43.2 ns         43.3 ns     16592593
hstring_find_long/1                 61.0 ns         61.4 ns     11200000
hstring_find_long/0                 41.4 ns         40.8 ns     17230769
hstring_find_long/1                 62.6 ns         62.8 ns     11200000
hstring_find_kmp_long/0              810 ns          795 ns       746667
hstring_find_kmp_long/0              849 ns          837 ns       746667
hstring_find_kmp_long/1             1018 ns         1025 ns       640000
std_string_compare_long/0           33.7 ns         33.7 ns     21333333
std_string_compare_long/1           41.3 ns         41.7 ns     17230769
std_string_compare_long/0           33.8 ns         33.8 ns     20363636
std_string_compare_long/1           39.5 ns         39.2 ns     17920000
hstring_compare_long/0              45.9 ns         46.0 ns     14933333
hstring_compare_long/1              49.9 ns         50.2 ns     11200000
hstring_compare_long/0              50.3 ns         50.6 ns     15448276
hstring_compare_long/1              49.9 ns         47.4 ns     11200000
std_string_iterate_long/0            949 ns          952 ns       640000
std_string_iterate_long/1          15193 ns        15346 ns        44800
std_string_iterate_long/0            942 ns          942 ns       746667
std_string_iterate_long/1          15324 ns        15346 ns        44800
hstring_iterate_long/0              95.0 ns         96.3 ns      7466667
hstring_iterate_long/1              1311 ns         1283 ns       560000
hstring_iterate_long/0              96.2 ns         96.3 ns      7466667
hstring_iterate_long/1              1244 ns         1256 ns       560000
std_string_substr_long/0             500 ns          505 ns      1454377
std_string_substr_long/1             505 ns          502 ns      1120000
std_string_substr_long/0             506 ns          516 ns      1000000
std_string_substr_long/1             501 ns          502 ns      1120000
hstring_substr_long/0               36.7 ns         36.9 ns     19478261
hstring_substr_long/1               38.0 ns         37.7 ns     18666667
hstring_substr_long/0               36.8 ns         36.8 ns     20363636
hstring_substr_long/1               38.9 ns         39.3 ns     18666667
BM_MyStringSplitChar                6931 ns         6975 ns       112000
BM_MyStringSplitString              7513 ns         7499 ns        89600

```
* 性能对比图表：`std::string vs hstring`

![std::string vs hstring](./benchmark_comparison.png)

* **结论：`hstring` 的性能基本上是 std::string 的5倍（find, compare比标准库稍低）**



