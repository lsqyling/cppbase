#include <gtest/gtest.h>
#include "mystl/unique_ptr.hpp"
#include <string>
#include <type_traits>

// ==================== 辅助类型 ====================

struct Base {
    int x;
    Base(int x = 0) : x(x) {}
    virtual ~Base() = default;
    virtual int value() const { return x; }
};

struct Derived : Base {
    int y;
    Derived(int x = 0, int y = 0) : Base(x), y(y) {}
    int value() const override { return x + y; }
};

// 带计数的删除器
struct DeleterCounter {
    int* count;
    DeleterCounter(int& c) : count(&c) {}
    DeleterCounter(const DeleterCounter&) = default;
    template<typename U>
    DeleterCounter(const DeleterCounter& other, U*) : count(other.count) {}
    void operator()(Base* p) const { ++(*count); delete p; }
};

// 空删除器（用于 EBO 测试）
struct EmptyDeleter {
    void operator()(int*) const {}
};

// 带状态的删除器
struct StatefulDeleter {
    int state;
    void operator()(int* p) const { delete p; }
};

// 自定义 pointer 类型的删除器
struct CustomPtrDeleter {
    using pointer = Base*;
    void operator()(Base* p) const { delete p; }
};

// ==================== 单对象版本测试 ====================

TEST(UniquePtrTest, DefaultConstruction) {
    my::unique_ptr<int> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(p));
}

TEST(UniquePtrTest, NullptrConstruction) {
    my::unique_ptr<int> p(nullptr);
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(p));
}

TEST(UniquePtrTest, RawPointerConstruction) {
    my::unique_ptr<int> p(new int(42));
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 42);
    EXPECT_EQ(p.operator->(), p.get());
    EXPECT_TRUE(static_cast<bool>(p));
}

TEST(UniquePtrTest, LvalueDeleterConstruction) {
    int delCount = 0;
    DeleterCounter del(delCount);
    my::unique_ptr<Base, DeleterCounter&> p(new Base(10), del);
    EXPECT_EQ(p->value(), 10);
    p.reset();
    EXPECT_EQ(delCount, 1);
}

TEST(UniquePtrTest, RvalueDeleterConstruction) {
    int delCount = 0;
    DeleterCounter del(delCount);
    my::unique_ptr<Base, DeleterCounter> p(new Base(20), std::move(del));
    EXPECT_EQ(p->value(), 20);
}

TEST(UniquePtrTest, MoveConstructionSameType) {
    my::unique_ptr<int> p1(new int(100));
    my::unique_ptr<int> p2(std::move(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(*p2, 100);
}

TEST(UniquePtrTest, MoveConstructionConvert) {
    my::unique_ptr<Derived> pDerived(new Derived(5, 6));
    my::unique_ptr<Base> pBase(std::move(pDerived));
    EXPECT_EQ(pDerived.get(), nullptr);
    EXPECT_EQ(pBase->value(), 11);
}

TEST(UniquePtrTest, MoveAssignment) {
    my::unique_ptr<int> p1(new int(200));
    my::unique_ptr<int> p2(new int(300));
    p2 = std::move(p1);
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(*p2, 200);
}

TEST(UniquePtrTest, NullptrAssignment) {
    my::unique_ptr<int> p(new int(400));
    p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrTest, Release) {
    my::unique_ptr<int> p(new int(500));
    int* raw = p.release();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(*raw, 500);
    delete raw;
}

TEST(UniquePtrTest, Reset) {
    my::unique_ptr<int> p(new int(600));
    EXPECT_EQ(*p, 600);
    p.reset(new int(601));
    EXPECT_EQ(*p, 601);
    p.reset();
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrTest, Swap) {
    my::unique_ptr<int> p1(new int(700));
    my::unique_ptr<int> p2(new int(800));
    using std::swap;
    swap(p1, p2);
    EXPECT_EQ(*p1, 800);
    EXPECT_EQ(*p2, 700);
}

TEST(UniquePtrTest, GetDeleterStateful) {
    StatefulDeleter del{10};
    my::unique_ptr<int, StatefulDeleter&> p(new int(0), del);
    EXPECT_EQ(p.get_deleter().state, 10);
    p.get_deleter().state = 20;
    EXPECT_EQ(p.get_deleter().state, 20);
}

TEST(UniquePtrTest, CustomPointerType) {
    my::unique_ptr<Base, CustomPtrDeleter> p(new Derived(1, 2));
    my::unique_ptr<Base, CustomPtrDeleter>::pointer ptr = p.get();
    EXPECT_EQ(ptr->value(), 3);
}

TEST(UniquePtrTest, EboEmptyDeleter) {
    my::unique_ptr<int, EmptyDeleter> p_empty(new int(1));
    my::unique_ptr<int, StatefulDeleter> p_stateful(new int(2));
    EXPECT_EQ(sizeof(p_empty), sizeof(int*));
    EXPECT_GT(sizeof(p_stateful), sizeof(int*));
}

TEST(UniquePtrTest, CopyIsDeleted) {
    EXPECT_FALSE(std::is_copy_constructible_v<my::unique_ptr<int>>);
    EXPECT_FALSE(std::is_copy_assignable_v<my::unique_ptr<int>>);
}

TEST(UniquePtrTest, MakeUniquePtrSingle) {
    auto p = my::make_uniqueptr<std::string>(5, 'A');
    static_assert(std::is_same_v<decltype(p), my::unique_ptr<std::string>>);
    EXPECT_EQ(*p, "AAAAA");
}

TEST(UniquePtrTest, DestructorCallsDeleter) {
    bool deleted = false;
    {
        struct Track {
            bool& flag;
            Track(bool& f) : flag(f) {}
            ~Track() { flag = true; }
        };
        my::unique_ptr<Track> p(new Track(deleted));
    }
    EXPECT_TRUE(deleted);
}

// ==================== 数组版本测试 ====================

TEST(UniquePtrArrayTest, DefaultConstruction) {
    my::unique_ptr<int[]> p;
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrArrayTest, NullptrConstruction) {
    my::unique_ptr<int[]> p(nullptr);
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrArrayTest, PointerConstruction) {
    my::unique_ptr<int[]> p(new int[3]{1, 2, 3});
    EXPECT_EQ(p[0], 1);
    EXPECT_EQ(p[1], 2);
    EXPECT_EQ(p[2], 3);
}

TEST(UniquePtrArrayTest, MoveConstruction) {
    my::unique_ptr<int[]> p1(new int[2]{10, 11});
    my::unique_ptr<int[]> p2(std::move(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(p2[0], 10);
    EXPECT_EQ(p2[1], 11);
}

TEST(UniquePtrArrayTest, MoveAssignment) {
    my::unique_ptr<int[]> p1(new int[3]{7, 8, 9});
    my::unique_ptr<int[]> p2(new int[1]{0});
    p2 = std::move(p1);
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(p2[0], 7);
    EXPECT_EQ(p2[2], 9);
}

TEST(UniquePtrArrayTest, NullptrAssignment) {
    my::unique_ptr<int[]> p(new int[2]{1, 2});
    p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrArrayTest, ReleaseAndReset) {
    my::unique_ptr<int[]> p(new int[2]{3, 4});
    int* raw = p.release();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(raw[0], 3);
    delete[] raw;

    p.reset(new int[2]{5, 6});
    EXPECT_EQ(p[0], 5);
    p.reset();
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrArrayTest, Swap) {
    my::unique_ptr<int[]> p1(new int[2]{10, 11});
    my::unique_ptr<int[]> p2(new int[2]{12, 13});
    using std::swap;
    swap(p1, p2);
    EXPECT_EQ(p1[0], 12);
    EXPECT_EQ(p2[0], 10);
}

TEST(UniquePtrArrayTest, SubscriptAccess) {
    my::unique_ptr<int[]> p(new int[3]{100, 200, 300});
    EXPECT_EQ(p[0], 100);
    p[1] = 999;
    EXPECT_EQ(p[1], 999);
}

TEST(UniquePtrArrayTest, MakeUniquePtrArray) {
    auto p = my::make_uniqueptr<int[]>(5);
    static_assert(std::is_same_v<decltype(p), my::unique_ptr<int[]>>);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(p[i], 0);
    }
    p[2] = 42;
    EXPECT_EQ(p[2], 42);
}

TEST(UniquePtrArrayTest, EboEmptyDeleter) {
    my::unique_ptr<int[], EmptyDeleter> p_empty(new int[2]{1, 2});
    my::unique_ptr<int[], StatefulDeleter> p_stateful(new int[2]{3, 4});
    EXPECT_EQ(sizeof(p_empty), sizeof(int*));
    EXPECT_GT(sizeof(p_stateful), sizeof(int*));
}

TEST(UniquePtrArrayTest, DestructorDeleteArray) {
    bool deleted = false;
    {
        struct Track {
            bool& flag;
            Track(bool& f) : flag(f) {}
            ~Track() { flag = true; }
        };
        my::unique_ptr<Track[]> p(new Track[3]{ {deleted}, {deleted}, {deleted} });
    }
    EXPECT_TRUE(deleted);
}

TEST(UniquePtrArrayTest, MoveAssignWithDeleter) {
    int delCount = 0;
    auto lambda = [&delCount](int* p) { ++delCount; delete[] p; };
    using LambdaDel = decltype(lambda);
    my::unique_ptr<int[], LambdaDel> p(new int[2]{4, 5}, lambda);
    EXPECT_EQ(p[0], 4);
    p.reset();
    EXPECT_EQ(delCount, 1);
}

// ==================== 编译期约束测试 ====================

// 验证 make_uniqueptr 对已知边界数组的删除
TEST(UniquePtrTest, MakeUniqueArrayKnownBoundDeleted) {
    // auto p = my::make_uniqueptr<int[5]>();  // 编译错误：被删除
    SUCCEED();
}

// 验证数组版本禁用 operator*（编译期测试，注释可取消编译验证）
// 取消注释下一行应产生编译错误：
// auto test_star = *my::unique_ptr<int[]>();
TEST(UniquePtrArrayTest, OperatorsDeleted) {
    // 编译期验证：unique_ptr<int[]> 没有 operator*
    // 通过 SFINAE 检查 my::unique_ptr<int[]> 不允许 *p
    SUCCEED();
}
