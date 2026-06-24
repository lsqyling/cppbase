#ifndef MYSTL_HSTRING_HPP
#define MYSTL_HSTRING_HPP
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cstring>

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

    // ---------- 辅助函数 ----------
    bool is_small() const noexcept {
        return (m_size & (1ULL << (sizeof(size_type) * 8 - 1))) != 0;
    }

    size_type get_size() const noexcept {
        return m_size & ~(1ULL << (sizeof(size_type) * 8 - 1));
    }

    void set_size(size_type sz, bool small) noexcept {
        if (small)
            m_size = sz | (1ULL << (sizeof(size_type) * 8 - 1));
        else
            m_size = sz;
    }

    // 返回可修改的数据指针（无论大小）
    value_type * data_ptr() noexcept {
        if (is_small())
            return m_storage.local;
        else
            return m_storage.dynamic.data;
    }

    // 返回不可修改的数据指针（无论大小）
    const value_type * data_ptr() const noexcept {
        if (is_small())
            return m_storage.local;
        else
            return m_storage.dynamic.data;
    }

    // 返回当前容量（包括结尾 '\0' 的空间）
    size_type get_capacity() const noexcept {
        if (is_small())
            return small_capacity + 1;
        else
            return m_storage.dynamic.capacity;
    }

    // 设置容量（仅在大字符串时调用）
    void set_capacity(size_type cap) noexcept {
        m_storage.dynamic.capacity = cap;
    }

    // 销毁动态资源（如果存在），并将对象置为空（小字符串状态）
    void destroy() noexcept {
        if (!is_small() && m_storage.dynamic.data) {
            delete[] m_storage.dynamic.data;
            m_storage.dynamic.data = nullptr;   // 避免二次删除
        }
        // 重置为空字符串（小字符串）
        m_storage.local[0] = '\0';
        set_size(0, true);
    }

    // 重新分配内存：确保容量至少为 new_cap（包含结尾 '\0'）
    // 强异常安全保证：若抛出异常，原对象不变
    void reserve_impl(size_type new_cap) {
        if (new_cap <= get_capacity()) return;          // 容量足够

        // 分配新内存（new 可能抛出 std::bad_alloc）
        auto new_buf = new value_type [new_cap];

        // 复制旧数据（旧数据始终以 '\0' 结尾）
        size_type old_sz = get_size();
        if (old_sz > 0)
            std::memcpy(new_buf, data_ptr(), old_sz + 1);   // 包括结尾 '\0'

        // 至此所有可能抛出的操作已完成，可以安全修改内部状态
        if (!is_small()) {
            // 原来是大字符串，释放旧内存
            delete[] m_storage.dynamic.data;
        }

        // 更新为大字符串状态
        m_storage.dynamic.data = new_buf;
        m_storage.dynamic.capacity = new_cap;
        set_size(old_sz, false);   // 标志为大
    }

    void empty_construct() noexcept {
        m_storage.local[0] = '\0';
        set_size(0, true);
    }

public:
    // ---------- 构造 / 析构 ----------

    // 默认构造：空字符串（小字符串）
    string() noexcept {
        empty_construct();
    }

    // 从 std::string 构造
    string(const std::string &s) {
        size_type len = s.size();
        if (len == 0) {
            empty_construct();
            return ;
        }
        if (len <= small_capacity) {
            std::memcpy(m_storage.local, s.data(), len);
            set_size(len, true);
            m_storage.local[len] = '\0';
        } else {
            m_storage.dynamic.data = new value_type [len + 1];
            std::memcpy(m_storage.dynamic.data, s.data(), len);
            m_storage.dynamic.data[len] = '\0';
            m_storage.dynamic.capacity = len + 1;
            set_size(len, false);
        }
    }

    // 从 C 字符串构造（允许 nullptr 视为空）
    string(const value_type * s) {
        size_type len = std::strlen(s);
        // C 字符串为nullptr || len = 0
        if (s == nullptr || len == 0) {
            empty_construct();
            return ;
        }
        if (len <= small_capacity) {
            // 小字符串直接复制
            std::memcpy(m_storage.local, s, len);
            set_size(len, true);
            // 保持统一结尾'\0'
            m_storage.local[len] = '\0';
        } else {
            // 大字符串需要动态分配
            m_storage.dynamic.data = new value_type [len + 1];
            std::memcpy(m_storage.dynamic.data, s, len);
            m_storage.dynamic.capacity = len + 1;
            set_size(len, false);
            // 保持统一结尾'\0'
            m_storage.dynamic.data[len] = '\0';
        }
    }

    // 从 C 字符串 + 长度构造
    string(const char* s, size_type count) {
        if (s == nullptr && count > 0) throw std::invalid_argument("nullptr with positive count");
        if (s == nullptr || count == 0) {
            empty_construct();
            return ;
        }
        if (count <= small_capacity) {
            std::memcpy(m_storage.local, s, count);
            set_size(count, true);
            m_storage.local[count] = '\0';
        } else {
            m_storage.dynamic.data = new value_type [count + 1];
            std::memcpy(m_storage.dynamic.data, s, count);
            m_storage.dynamic.data[count] = '\0';
            m_storage.dynamic.capacity = count + 1;
            set_size(count, false);
        }
    }

    // 拷贝构造
    string(const string& other) {
        size_type sz = other.get_size();
        if (other.is_small()) {
            // 小字符串直接复制 local
            std::memcpy(m_storage.local, other.m_storage.local, sz + 1);
            set_size(sz, true);
        } else {
            // 大字符串：分配内存并复制
            m_storage.dynamic.data = new value_type [sz + 1];
            std::memcpy(m_storage.dynamic.data, other.m_storage.dynamic.data, sz + 1);
            m_storage.dynamic.capacity = sz + 1;
            set_size(sz, false);
        }
    }

    // 移动构造
    string(string&& other) noexcept {
        if (other.is_small()) {
            // 小字符串：逐个字符复制（不能窃取指针）
            size_type sz = other.get_size();
            std::memcpy(m_storage.local, other.m_storage.local, sz + 1);
            set_size(sz, true);
            // 将 other 置为空（小字符串）
            other.m_storage.local[0] = '\0';
            other.set_size(0, true);
        } else {
            // 大字符串：窃取资源
            m_storage.dynamic.data = other.m_storage.dynamic.data;
            m_storage.dynamic.capacity = other.m_storage.dynamic.capacity;
            set_size(other.get_size(), false);
            // 将 other 置为空（小字符串状态，避免释放）
            other.m_storage.local[0] = '\0';
            other.set_size(0, true);
            // 注意：other 原先的 data 已被窃取，无需释放
        }
    }

    // 析构函数
    ~string() noexcept {
        if (!is_small() && m_storage.dynamic.data) {
            delete[] m_storage.dynamic.data;
        }
    }

    // ---------- 赋值运算符 ----------

    // 拷贝赋值（使用 copy-and-swap，强异常安全）
    string& operator=(const string& other) {
        if (this != &other) {
            string temp(other);       // 拷贝构造（可能抛出，但原对象不变）
            swap(temp);                // 交换，noexcept
        }
        return *this;
    }

    // 移动赋值（简单交换资源，noexcept）
    string& operator=(string&& other) noexcept {
        if (this != &other) {
            // 先清除当前资源
            destroy();
            // 窃取 other 的资源
            if (other.is_small()) {
                size_type sz = other.get_size();
                std::memcpy(m_storage.local, other.m_storage.local, sz + 1);
                set_size(sz, true);
            } else {
                m_storage.dynamic.data = other.m_storage.dynamic.data;
                m_storage.dynamic.capacity = other.m_storage.dynamic.capacity;
                set_size(other.get_size(), false);
            }
            // 将 other 置为空（小字符串）
            other.m_storage.local[0] = '\0';
            other.set_size(0, true);
        }
        return *this;
    }

    // 从 C 字符串赋值
    string& operator=(const char* s) {
        *this = string(s);    // 利用拷贝赋值（构造临时对象，然后交换）
        return *this;
    }

    // ---------- 元素访问 ----------

    reference operator[](size_type pos) noexcept {
        return data_ptr()[pos];
    }

    const_reference operator[](size_type pos) const noexcept {
        return data_ptr()[pos];
    }

    reference at(size_type pos) {
        if (pos >= get_size())
            throw std::out_of_range("my::string::at");
        return data_ptr()[pos];
    }

    const_reference at(size_type pos) const {
        if (pos >= get_size())
            throw std::out_of_range("my::string::at");
        return data_ptr()[pos];
    }

    reference front() noexcept {
        return data_ptr()[0];
    }

    const_reference front() const noexcept {
        return data_ptr()[0];
    }

    reference back() noexcept {
        return data_ptr()[get_size() - 1];
    }

    const_reference back() const noexcept {
        return data_ptr()[get_size() - 1];
    }

    const_pointer c_str() const noexcept {
        return data_ptr();
    }

    const_pointer data() const noexcept {
        return data_ptr();
    }

    // ---------- 迭代器 ----------

    iterator begin() noexcept {
        return data_ptr();
    }

    const_iterator begin() const noexcept {
        return data_ptr();
    }

    const_iterator cbegin() const noexcept {
        return data_ptr();
    }

    iterator end() noexcept {
        return data_ptr() + get_size();
    }

    const_iterator end() const noexcept {
        return data_ptr() + get_size();
    }

    const_iterator cend() const noexcept {
        return data_ptr() + get_size();
    }

    // ---------- 容量 ----------

    size_type size() const noexcept {
        return get_size();
    }

    size_type length() const noexcept {
        return get_size();
    }

    size_type capacity() const noexcept {
        return get_capacity();
    }

    bool empty() const noexcept {
        return get_size() == 0;
    }

    void reserve(size_type new_cap) {
        // new_cap 是用户期望的容量（不包括结尾 '\0', 通常指可存储字符数，不包括 '\0'
        // 但内部容量包括 '\0', 所以需要 +1
        if (new_cap <= get_capacity() - 1) return;   // 已有足够空间
        reserve_impl(new_cap + 1);                    // 确保有 new_cap+1 字节
    }

    void shrink_to_fit() {
        // 小字符串无法缩小
        if (is_small()) return;
        size_type sz = get_size();
        if (sz <= small_capacity) {
            // 可以转为小字符串
            value_type  temp[small_capacity + 1];
            std::memcpy(temp, m_storage.dynamic.data, sz + 1);
            delete[] m_storage.dynamic.data;
            std::memcpy(m_storage.local, temp, sz + 1);
            set_size(sz, true);
        } else {
            // 仍为大字符串，但可以缩小容量到恰好 sz+1
            if (m_storage.dynamic.capacity > sz + 1) {
                auto  new_buf = new value_type [sz + 1];
                std::memcpy(new_buf, m_storage.dynamic.data, sz + 1);
                delete[] m_storage.dynamic.data;
                m_storage.dynamic.data = new_buf;
                m_storage.dynamic.capacity = sz + 1;
                // 大小和标志不变
            }
        }
    }

    // ---------- 修改操作 ----------

    void clear() noexcept {
        if (is_small()) {
            m_storage.local[0] = '\0';
            set_size(0, true);
        } else {
            // 保持大字符串状态，但将 size 置 0，第一个字符设为 '\0'
            m_storage.dynamic.data[0] = '\0';
            set_size(0, false);
        }
    }

    void push_back(char ch) {
        size_type old_sz = get_size();
        size_type new_sz = old_sz + 1;
        if (new_sz <= get_capacity() - 1) {   // 容量足够（不包括 '\0' ）
            // 直接追加
            data_ptr()[old_sz] = ch;
            data_ptr()[new_sz] = '\0';
            set_size(new_sz, is_small());
        } else {
            // 需要扩容，先计算新容量
            size_type new_cap = std::max(2 * get_capacity(), new_sz + 1);
            // 使用 reserve_impl（强异常安全）
            reserve_impl(new_cap);
            // 现在容量足够，追加字符
            data_ptr()[old_sz] = ch;
            data_ptr()[new_sz] = '\0';
            set_size(new_sz, false);   // 此时一定是大字符串
        }
    }

    void pop_back() {
        size_type sz = get_size();
        // when sz == 0, do nothing.
        if (sz == 0) return;
        data_ptr()[sz - 1] = '\0';
        set_size(sz - 1, is_small());
    }

    string& append(const string& str) {
        return append(str.data(), str.size());
    }

    string& append(const char* s, size_type count) {
        if (count == 0) return *this;
        size_type old_sz = get_size();
        size_type new_sz = old_sz + count;

        // 检查容量是否足够（包括 '\0' ）
        if (new_sz <= get_capacity() - 1) {
            // 直接追加
            std::memcpy(data_ptr() + old_sz, s, count);
            data_ptr()[new_sz] = '\0';
            set_size(new_sz, is_small());
        } else {
            // 需要扩容
            size_type new_cap = std::max(2 * get_capacity(), new_sz + 1);
            // 先分配新内存（强异常安全）
            value_type * new_buf = new value_type [new_cap];
            // 复制旧数据
            if (old_sz > 0)
                std::memcpy(new_buf, data_ptr(), old_sz);
            // 追加新数据
            std::memcpy(new_buf + old_sz, s, count);
            new_buf[new_sz] = '\0';

            // 安全切换
            if (!is_small())
                delete[] m_storage.dynamic.data;
            m_storage.dynamic.data = new_buf;
            m_storage.dynamic.capacity = new_cap;
            set_size(new_sz, false);
        }
        return *this;
    }

    string& operator+=(const string& str) {
        return append(str);
    }

    string& operator+=(char ch) {
        push_back(ch);
        return *this;
    }

    // 插入、擦除、替换等可类似实现，此处仅给出简单 erase 示例
    string& erase(size_type pos = 0, size_type count = npos) {
        size_type sz = get_size();
        if (pos > sz) throw std::out_of_range("my::string::erase");
        if (count > sz - pos) count = sz - pos;
        if (count == 0) return *this;

        value_type * p = data_ptr();
        // 移动后续字符（包括 '\0' ）向前
        std::memmove(p + pos, p + pos + count, sz - pos - count + 1);
        set_size(sz - count, is_small());
        return *this;
    }
    string &insert(size_type pos, const string &str) {
        return insert(pos, str.data(), str.size());
    }

    string &insert(size_type pos, const char *s) {
        return insert(pos, s, std::strlen(s));
    }

    string &insert(size_type pos, const char *s, size_type count) {
        size_type sz = get_size();
        if (pos > sz) {
            throw std::out_of_range("my::string::insert");
        }
        if (count == 0) return *this;

        size_type new_sz = sz + count;
        // 检查容量是否足够
        if (new_sz <= get_capacity() - 1) {
            // 容量足够，直接在原内存上操作
            auto p = data_ptr();
            // 将 [pos, old_sz) 的内容向后移动 count 个位置，包括结尾 '\0'
            // 注意：移动区域可能有重叠，使用 std::memove
            std::memmove(p + pos + count, p + pos, sz - pos + 1);
            // 复制新内容
            std::memcpy(p + pos + count, s, count);
            set_size(new_sz, is_small());
        } else {
            // 需要扩容，强异常安全方式：先分配新内存，再切换
            size_type new_cap = std::max(2 * get_capacity(), new_sz + 1);
            auto new_buf = new value_type[new_cap];

            // 分段复制旧数据和新数据
            // 1. 复制 [0, pos)
            if (pos > 0)
                std::memcpy(new_buf, data_ptr(), pos);
            // 2. 复制插入内容
            std::memcpy(new_buf + pos, s, count);
            // 3. 复制 [pos, old_sz) 及结尾 null
            std::memcpy(new_buf + pos + count, data_ptr() + pos, sz - pos + 1);

            // 安全切换
            if (!is_small())
                delete[] m_storage.dynamic.data;
            m_storage.dynamic.data = new_buf;
            m_storage.dynamic.capacity = new_cap;
            set_size(new_sz, false);
        }
        return *this;
    }
    // 4. 在迭代器位置 p 之前插入一个字符 ch，返回指向插入字符的迭代器
    iterator insert(const_iterator p, value_type ch) {
        return insert(p, 1, ch);
    }

    // 在迭代器位置 p 之前插入 count 个相同字符 ch，返回指向第一个插入字符的迭代器
    iterator insert(const_iterator p, size_type count, value_type ch) {
        if (count == 0)
            return const_cast<iterator>(p); // 返回原始位置的迭代器

        size_type pos = p - cbegin();
        if (pos > get_size())
            throw std::out_of_range("my::string::insert(iterator, count)");

        size_type old_sz = get_size();
        size_type new_sz = old_sz + count;

        if (new_sz <= get_capacity() - 1) {
            // 容量足够
            pointer ptr = data_ptr();
            std::memmove(ptr + pos + count, ptr + pos, old_sz - pos + 1); // 包括 '\0'
            // 填充 count 个 ch
            for (size_type i = 0; i < count; ++i)
                ptr[pos + i] = ch;
            set_size(new_sz, is_small());
            return begin() + pos;
        } else {
            // 需要扩容
            size_type new_cap = std::max(2 * get_capacity(), new_sz + 1);
            auto new_buf = new value_type[new_cap];

            // 分段复制
            if (pos > 0)
               std::memcpy(new_buf, data_ptr(), pos);
            // 填充插入的字符
            for (size_type i = 0; i < count; ++i)
                new_buf[pos + i] = ch;
            std::memcpy(new_buf + pos + count, data_ptr() + pos, old_sz - pos + 1);

            if (!is_small())
                delete[] m_storage.dynamic.data;
            m_storage.dynamic.data = new_buf;
            m_storage.dynamic.capacity = new_cap;
            set_size(new_sz, false);
            return begin() + pos;
        }
    }

    // 交换
    void swap(string& other) noexcept {
        using std::swap;
        // 如果两者都是小字符串或都是大字符串，可以直接交换内部存储
        // 但由于有 union，简单的方法：先转换为两个独立 string，再交换成员
        // 但为了 noexcept，我们可以按字节交换整个对象（通过 memcpy），
        // 但这样可能破坏自包含性。更好的方法是分别处理标志和存储。
        // 由于我们实现了移动构造和移动赋值，可以借助临时变量：
        // 但直接按成员交换是最简单的：
        if (this == &other) return;

        // 交换标志和大小
        swap(m_size, other.m_size);
        // 交换存储（由于是 union，整体交换）
        value_type  tmp[sizeof(Storage)];
        std::memcpy(tmp, &m_storage, sizeof(Storage));
        std::memcpy(&m_storage, &other.m_storage, sizeof(Storage));
        std::memcpy(&other.m_storage, tmp, sizeof(Storage));
    }

    // ---------- 查找 ----------
    size_type find(const string& str, size_type pos = 0) const noexcept {
        return find(str.data(), pos, str.size());
    }


    // 使用 KMP 算法查找子串 str，从位置 pos 开始
    size_type find_kmp(const string& str, size_type pos = 0) const {
        return find_kmp(str.data(), pos, str.size());
    }

    // 使用 KMP 算法查找子串 s（长度为 count），从位置 pos 开始
    size_type find_kmp(const_pointer s, size_type pos, size_type count) const {
        // 边界检查
        if (pos > get_size()) return npos;
        if (count == 0) return pos;                // 空串始终匹配在 pos 处

        size_type n = get_size() - pos;            // 文本串剩余长度
        if (count > n) return npos;                 // 模式串比文本长，不可能匹配

        const_pointer text = data_ptr() + pos;      // 文本串起始指针
        // KMP 预处理：计算 next 数组（失配时跳转位置）
        // 使用动态数组存储 next 值，可能抛出 std::bad_alloc
        // next: 不含当前，前面字符串-前后缀最大匹配长度（不能是整体）
        std::vector<int> next(count, 0);
        if (count == 1) {
            next[0] = -1;
            goto FindKMP;
        }
        next[0] = -1;
        next[1] = 0;
        // 第一遍：标准 LPS
        for (int i = 2, prev = 0; i < count; ) {
            if (s[i-1] == s[prev]) {
                next[i++] = ++prev;
            } else if (prev > 0) {
                prev = next[prev];
            } else {
                next[i++] = 0;
            }
        }

        FindKMP:
        // KMP 搜索
        int i = 0;          // 文本串索引
        int j = 0;          // 模式串索引
        while (i < n && j < count) {
            if (text[i] == s[j]) {
                ++i;++j;
            } else if (j == 0) {
                ++i;
            } else {
                j = next[j];
            }
        }
        return j == count ? pos + (i - j) : npos;
    }

    size_type find(const char* s, size_type pos, size_type count) const noexcept {
        // 检查起始搜索位置 pos 是否超出字符串长度。若超出，根据标准直接返回 npos（未找到）。
        if (pos > get_size()) return npos;

        // 如果要查找的子串长度 count 为 0（即查找空串），则空串被认为在任意位置匹配。
        // 由于上一步已确保 pos <= size()，因此直接返回 pos（即 pos 位置匹配）。
        if (count == 0) return pos <= get_size() ? pos : npos;

        // 获取从 pos 开始的内存指针，指向待搜索区域的起始位置（“干草堆”）。
        const value_type * haystack = data_ptr() + pos;

        // 计算从 pos 到字符串末尾的剩余字符数（干草堆的长度）。
        size_t hay_len = get_size() - pos;

        // 如果要查找的子串长度 count 大于剩余长度，则不可能匹配，直接返回 npos。
        if (count > hay_len) return npos;

        // 使用 memchr 在干草堆中查找子串的第一个字符 s[0] 首次出现的位置。
        // 返回指向该字符的指针，若未找到则返回 nullptr，并强制转换为 value_type* 类型。
        const value_type * found = static_cast<const value_type *>(std::memchr(haystack, s[0], hay_len));

        // 循环处理所有找到的 s[0] 出现位置
        while (found) {
            // 计算找到的位置相对于整个字符串起始位置的偏移量（全局索引）。
            size_t offset = found - data_ptr();

            // 检查从 offset 开始的子串是否会超出字符串总长度，并使用 memcmp 比较完整子串是否匹配。
            // offset + count <= get_size() 用于防止越界（虽然理论上不会发生，但安全起见保留检查）。
            if (offset + count <= get_size() &&
                std::memcmp(found, s, count) == 0) {
                // 如果完整匹配，返回匹配子串的起始索引。
                return offset;
            }

            // 如果当前 found 位置不匹配（可能第一个字符匹配但后续字符不同），则继续在剩余区域搜索。
            // 更新 found 为从上一个匹配位置的下一个字符开始，再次查找 s[0] 的下一次出现。
            // 剩余长度计算：总搜索长度 hay_len 减去已跳过的部分 (offset - pos) 再减去当前字符占用的 1。
            found = static_cast<const value_type *>(std::memchr(found + 1, s[0], hay_len - (offset - pos) - 1));
        }

        // 所有可能位置都检查完毕，未找到完整匹配，返回 npos。
        return npos;
    }

    // ---------- 比较 ----------
    int compare(const string& other) const noexcept {
        size_type lhs_sz = get_size();
        size_type rhs_sz = other.get_size();
        int cmp = std::memcmp(data_ptr(), other.data_ptr(), std::min(lhs_sz, rhs_sz));
        if (cmp != 0) return cmp;
        if (lhs_sz < rhs_sz) return -1;
        if (lhs_sz > rhs_sz) return 1;
        return 0;
    }

    // ---------- 子串 ----------
    string substr(size_type pos = 0, size_type count = npos) const {
        if (pos > get_size()) throw std::out_of_range("my::string::substr");
        size_type rcount = std::min(count, get_size() - pos);
        return string(data_ptr() + pos, rcount);
    }
    // ---------- 分隔 ----------
    bool is_whitespace_only(const string &token)
    {
        return std::all_of(token.begin(), token.end(), [](unsigned char c) {
            return std::isspace(c);
        });
    }
    std::vector<string> split(char delimiter, bool skip_empty = true)
    {
        std::vector<string> tokens;
        string token;
        for (auto c : *this) {
            if (c == delimiter) {
                if (!skip_empty || !is_whitespace_only(token)) {
                    tokens.push_back(token);
                }
                token.clear();
            } else {
                token += c;
            }
        }
        if (!skip_empty || !is_whitespace_only(token)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::vector<string> split(const string &delimiter, bool skip_empty = true)
    {
        std::vector<string> tokens;
        size_t start = 0;
        size_t end = find(delimiter);

        while (end != string::npos) {
            auto token = substr(start, end - start);
            if (!skip_empty || !is_whitespace_only(token)) {
                tokens.push_back(token);
            }
            start = end + delimiter.size();
            end = find(delimiter, start);
        }
        string token = substr(start);
        if (!skip_empty || !is_whitespace_only(token)) {
            tokens.push_back(token);
        }

        return tokens;
    }

};

// ---------- 非成员运算符重载 ----------

inline bool operator==(const string& lhs, const string& rhs) noexcept {
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(const string& lhs, const string& rhs) noexcept {
    return !(lhs == rhs);
}

inline bool operator<(const string& lhs, const string& rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

inline bool operator<=(const string& lhs, const string& rhs) noexcept {
    return lhs.compare(rhs) <= 0;
}

inline bool operator>(const string& lhs, const string& rhs) noexcept {
    return lhs.compare(rhs) > 0;
}

inline bool operator>=(const string& lhs, const string& rhs) noexcept {
    return lhs.compare(rhs) >= 0;
}

// 与 C 字符串的比较
inline bool operator==(const string& lhs, const char* rhs) {
    return lhs == string(rhs);
}
inline bool operator==(const char* lhs, const string& rhs) {
    return string(lhs) == rhs;
}
// ... 其他比较运算符可类似定义

inline string operator+(const string& lhs, const string& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}
inline string operator+(const string& lhs, const char* rhs) {
    return lhs + string(rhs);
}
inline string operator+(const char* lhs, const string& rhs) {
    return string(lhs) + rhs;
}
inline string operator+(const string& lhs, char rhs) {
    string tmp{lhs};
    tmp += rhs;
    return tmp;
}
inline string operator+(char lhs, const string& rhs) {
    string result;
    result.push_back(lhs);
    result += rhs;
    return result;
}

// 输出流支持
inline std::ostream& operator<<(std::ostream& os, const string& str) {
    return os.write(str.data(), str.size());
}
} // namespace my

using hstring = my::string;

#endif