#ifndef MYSTL_UNIQUE_PTR_HPP
#define MYSTL_UNIQUE_PTR_HPP
#include <concepts>
#include <type_traits>
#include <utility>
#include <cstddef>

namespace my {

// -------------------- default_delete 系列 --------------------
template<typename T>
struct default_delete
{
    constexpr default_delete() noexcept = default;

    template<typename U>
    requires std::convertible_to<U *, T *>
    constexpr default_delete(const default_delete<U> &) noexcept {}


    constexpr void operator()(T *ptr) const noexcept { delete ptr; }
};

template<typename T>
struct default_delete<T[]>
{
    constexpr default_delete() noexcept = default;

    template<typename U>
    requires std::convertible_to<U(*)[], T(*)[]>
    constexpr default_delete(const default_delete<U[]> &) noexcept {}

    template<typename U>
    requires std::convertible_to<U (*)[], T (*)[]>
    constexpr void operator()(U *ptr) const noexcept { delete[] ptr; }
};

// -------------------- compressed_pair 实现（EBO） --------------------
namespace detail {
template <typename D, typename P, bool = std::is_empty_v<D> && !std::is_final_v<D>>
class compressed_pair {
protected:
    D first;
    P second;
public:
    constexpr compressed_pair() noexcept
    requires std::default_initializable<D>
            : first(), second() {}

    constexpr compressed_pair(const D& d, const P& p) noexcept
    requires std::copy_constructible<D>
            : first(d), second(p) {}

    constexpr compressed_pair(D&& d, const P& p) noexcept
    requires (!std::is_reference_v<D> && std::move_constructible<D>)
            : first(std::move(d)), second(p) {}

    constexpr D& get_first() noexcept { return first; }
    constexpr const D& get_first() const noexcept { return first; }
    constexpr P& get_second() noexcept { return second; }
    constexpr const P& get_second() const noexcept { return second; }
};

// 特化：空删除器（可继承）
template <typename D, typename P>
class compressed_pair<D, P, true> : private D {
protected:
    P second;
public:
    constexpr compressed_pair() noexcept
    requires std::default_initializable<D>
            : D(), second() {}

    constexpr compressed_pair(const D& d, const P& p) noexcept
    requires std::copy_constructible<D>
            : D(d), second(p) {}

    constexpr compressed_pair(D&& d, const P& p) noexcept
    requires (!std::is_reference_v<D> && std::move_constructible<D>)
            : D(std::move(d)), second(p) {}

    constexpr D& get_first() noexcept { return *this; }
    constexpr const D& get_first() const noexcept { return *this; }
    constexpr P& get_second() noexcept { return second; }
    constexpr const P& get_second() const noexcept { return second; }
};
} // namespace detail

// -------------------- 获取 pointer 类型的 traits --------------------
namespace detail {
template <typename D, typename T, typename = void>
struct pointer_type {
    using type = T*;  // 默认：T*
};
template <typename D, typename T>
struct pointer_type<D, T, std::void_t<typename D::pointer>> {
    using type = typename D::pointer; // 删除器定义了 pointer
};
}

// -------------------- 主模板：单对象 --------------------
template <typename T, typename D = default_delete<T>>
class unique_ptr {

public:
    using element_type = T;
    using deleter_type = D;
    using pointer = typename detail::pointer_type<D, T>::type;

    // ----- 构造函数 -----
    constexpr unique_ptr() noexcept
    requires std::default_initializable<D>
            : pair() {}

    constexpr unique_ptr(std::nullptr_t) noexcept
    requires std::default_initializable<D>
            : pair() {}

    explicit constexpr unique_ptr(pointer p) noexcept
    requires std::default_initializable<D>
            : pair(D(), p) {}

    // 左值删除器构造
    constexpr unique_ptr(pointer p, const D& d) noexcept
    requires std::constructible_from<D, const D&>
            : pair(d, p) {}

    // 右值删除器构造（仅非引用D）
    constexpr unique_ptr(pointer p, D&& d) noexcept
    requires (!std::is_reference_v<D>) && std::move_constructible<D>
            : pair(std::move(d), p) {}

    // 移动构造（同类型）
    template <typename Del = D>
    requires std::move_constructible<D>
    constexpr unique_ptr(unique_ptr&& other) noexcept
            : pair(std::move(other.pair.get_first()), other.release()) {}

    // 转换移动构造（从其他可转换的 unique_ptr）
    template <typename U, typename E>
    requires (!std::is_array_v<U>) &&   // 源不是数组
    std::convertible_to<typename unique_ptr<U, E>::pointer, pointer> &&
    (std::is_reference_v<D> ? std::is_same_v<E, D> : std::convertible_to<E, D>)
    constexpr unique_ptr(unique_ptr<U, E>&& other) noexcept
            : pair(std::forward<E>(other.pair.get_first()), other.release()) {}

    // 禁用拷贝
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    // ----- 析构函数 -----
    constexpr ~unique_ptr() noexcept {
        if (pair.get_second()) {
            pair.get_first()(pair.get_second());
        }
    }

    // ----- 赋值 -----
    constexpr unique_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    template <typename Del = D>
    requires std::is_move_assignable_v<D>
    constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
        reset(other.release());
        pair.get_first() = std::forward<D>(other.pair.get_first());
        return *this;
    }

    template <typename U, typename E>
    requires (!std::is_array_v<U>) && std::assignable_from<D&, E> &&
    std::convertible_to<typename unique_ptr<U, E>::pointer, pointer>
    constexpr unique_ptr& operator=(unique_ptr<U, E>&& other) noexcept {
        reset(other.release());
        pair.get_first() = std::forward<E>(other.pair.get_first());
        return *this;
    }

    // ----- 修改器 -----
    constexpr pointer release() noexcept {
        return std::exchange(pair.get_second(), nullptr);
    }

    constexpr void reset(pointer p = pointer()) noexcept {
        pointer old = std::exchange(pair.get_second(), p);
        if (old) {
            pair.get_first()(old);
        }
    }

    constexpr void swap(unique_ptr& other) noexcept {
        using std::swap;
        swap(pair.get_second(), other.pair.get_second());
        swap(pair.get_first(), other.pair.get_first());
    }


    // ----- 观察器 -----
    constexpr D& get_deleter() noexcept { return pair.get_first(); }
    constexpr const D& get_deleter() const noexcept { return pair.get_first(); }

    constexpr pointer get() const noexcept { return pair.get_second(); }

    constexpr std::add_lvalue_reference_t<T> operator*() const noexcept {
        return *pair.get_second();
    }

    constexpr pointer operator->() const noexcept {
        return pair.get_second();
    }

    constexpr explicit operator bool() const noexcept {
        return static_cast<bool>(pair.get_second());
    }

private:
    // 友元声明允许其他 unique_ptr 实例访问私有成员
    // 主要用于支持不同模板参数间的转换构造和赋值操作
    template<typename, typename>
    friend class unique_ptr;

    detail::compressed_pair<D, pointer> pair;
};

// -------------------- 数组偏特化 --------------------
template <typename T, typename D>
class unique_ptr<T[], D> {

public:
    using element_type = T;
    using deleter_type = D;
    using pointer = typename detail::pointer_type<D, T>::type;

    // ----- 构造函数 -----
    constexpr unique_ptr() noexcept
    requires std::default_initializable<D>
            : pair() {}

    constexpr unique_ptr(std::nullptr_t) noexcept
    requires std::default_initializable<D>
            : pair() {}

    explicit constexpr unique_ptr(pointer p) noexcept
    requires std::default_initializable<D>
            : pair(D(), p) {}

    template <typename Del = D>
    requires std::constructible_from<D, const Del&>
    constexpr unique_ptr(pointer p, const D& d) noexcept
            : pair(d, p) {}

    template <typename Del = D>
    requires (!std::is_reference_v<Del>) && std::constructible_from<D, Del&&>
    constexpr unique_ptr(pointer p, D&& d) noexcept
            : pair(std::move(d), p) {}

    // 移动构造（同类型）
    template <typename Del = D>
    requires std::move_constructible<D>
    constexpr unique_ptr(unique_ptr&& other) noexcept
            : pair(std::move(other.pair.get_first()), other.release()) {}

    // 禁用拷贝
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    // ----- 析构函数 -----
    constexpr ~unique_ptr() noexcept {
        if (pair.get_second()) {
            pair.get_first()(pair.get_second());
        }
    }

    // ----- 赋值 -----
    constexpr unique_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    template <typename Del = D>
    requires std::is_move_assignable_v<D>
    constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
        reset(other.release());
        pair.get_first() = std::forward<D>(other.pair.get_first());
        return *this;
    }

    template <typename U, typename E>
    requires std::is_array_v<U> &&
             std::assignable_from<D&, E> &&
             std::convertible_to<typename unique_ptr<U, E>::pointer, pointer>
    constexpr unique_ptr& operator=(unique_ptr<U, E>&& other) noexcept {
        reset(other.release());
        pair.get_first() = std::forward<E>(other.pair.get_first());
        return *this;
    }

    // ----- 修改器 -----
    constexpr pointer release() noexcept {
        return std::exchange(pair.get_second(), pointer());
    }

    constexpr void reset(pointer p = pointer()) noexcept {
        pointer old = std::exchange(pair.get_second(), p);
        if (old) {
            pair.get_first()(old);
        }
    }

    constexpr void swap(unique_ptr& other) noexcept {
        using std::swap;
        swap(pair.get_second(), other.pair.get_second());
        swap(pair.get_first(), other.pair.get_first());
    }

    // ----- 观察器 -----
    constexpr D& get_deleter() noexcept { return pair.get_first(); }
    constexpr const D& get_deleter() const noexcept { return pair.get_first(); }

    constexpr pointer get() const noexcept { return pair.get_second(); }

    // 数组版本：禁用 operator* 和 operator->
    constexpr std::add_lvalue_reference_t<T> operator*() const noexcept = delete;
    constexpr pointer operator->() const noexcept = delete;

    constexpr explicit operator bool() const noexcept {
        return static_cast<bool>(pair.get_second());
    }

    // 提供下标访问
    constexpr T& operator[](std::size_t i) const {
        return pair.get_second()[i];
    }

private:
    // 友元声明允许其他 unique_ptr 实例访问私有成员
    // 主要用于支持不同模板参数间的转换构造和赋值操作
    template<typename, typename>
    friend class unique_ptr;

    detail::compressed_pair<D, pointer> pair;
};

// -------------------- 非成员 swap --------------------
template <typename T, typename D>
constexpr void swap(unique_ptr<T, D>& lhs, unique_ptr<T, D>& rhs) noexcept {
    lhs.swap(rhs);
}
// make_uniqueptr for single objects
template<typename T, typename... Args>
requires (!std::is_array_v<T>)
my::unique_ptr<T> make_uniqueptr(Args&&... args) {
    return my::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// make_uniqueptr for arrays of unknown bound
template<typename T>
requires (std::is_array_v<T> && std::extent_v<T> == 0)
my::unique_ptr<T> make_uniqueptr(std::size_t size) {
    using U = std::remove_extent_t<T>;
    return my::unique_ptr<T>(new U[size]()); // value-initialization
}

// disallow make_uniqueptr for arrays of known bound
template<typename T>
requires (std::is_array_v<T> && std::extent_v<T> != 0)
void make_uniqueptr() = delete;

} // namespace my

#endif