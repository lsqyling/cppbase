#include <benchmark/benchmark.h>
#include <string>
#include <random>
#include <algorithm>
#include "mystl/hstring.hpp"

// 辅助函数：生成随机字符串
static std::string random_string(size_t length) {
    static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset)-2);
    std::string str(length, '\0');
    std::generate_n(str.begin(), length, [&]() { return charset[dist(rng)]; });
    return str;
}

// 测试数据
static const std::string short_src = "hello";
static const std::string long_src = random_string(100);  // 长度 > 15
static const std::string insert_payload = "xyz";
static const std::string find_pattern = "world";

// 构造测试
template <typename StringType>
static void BM_ConstructFromCString(benchmark::State& state) {
    const char* src = state.range(0) == 0 ? short_src.c_str() : long_src.c_str();
    for (auto _ : state) {
        StringType s(src);
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK_TEMPLATE(BM_ConstructFromCString, std::string)->Arg(0)->Arg(1)->Name("std_string_construct_short")->Arg(0)->Name("std_string_construct_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_ConstructFromCString, hstring)->Arg(0)->Arg(1)->Name("hstring_construct_short")->Arg(0)->Name("hstring_construct_long")->Arg(1);

template <typename StringType>
static void BM_CopyConstruct(benchmark::State& state) {
    StringType src(state.range(0) == 0 ? short_src : long_src);
    for (auto _ : state) {
        StringType dest(src);
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK_TEMPLATE(BM_CopyConstruct, std::string)->Arg(0)->Arg(1)->Name("std_string_copy_short")->Arg(0)->Name("std_string_copy_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_CopyConstruct, hstring)->Arg(0)->Arg(1)->Name("hstring_copy_short")->Arg(0)->Name("hstring_copy_long")->Arg(1);

template <typename StringType>
static void BM_MoveConstruct(benchmark::State& state) {
    for (auto _ : state) {
        StringType src(state.range(0) == 0 ? short_src : long_src);
        StringType dest(std::move(src));
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK_TEMPLATE(BM_MoveConstruct, std::string)->Arg(0)->Arg(1)->Name("std_string_move_short")->Arg(0)->Name("std_string_move_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_MoveConstruct, hstring)->Arg(0)->Arg(1)->Name("hstring_move_short")->Arg(0)->Name("hstring_move_long")->Arg(1);

// 赋值测试
template <typename StringType>
static void BM_CopyAssign(benchmark::State& state) {
    StringType src(state.range(0) == 0 ? short_src : long_src);
    StringType dest;
    for (auto _ : state) {
        dest = src;
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK_TEMPLATE(BM_CopyAssign, std::string)->Arg(0)->Arg(1)->Name("std_string_copy_assign_short")->Arg(0)->Name("std_string_copy_assign_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_CopyAssign, hstring)->Arg(0)->Arg(1)->Name("hstring_copy_assign_short")->Arg(0)->Name("hstring_copy_assign_long")->Arg(1);

template <typename StringType>
static void BM_MoveAssign(benchmark::State& state) {
    StringType dest;
    for (auto _ : state) {
        StringType src(state.range(0) == 0 ? short_src : long_src);
        dest = std::move(src);
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK_TEMPLATE(BM_MoveAssign, std::string)->Arg(0)->Arg(1)->Name("std_string_move_assign_short")->Arg(0)->Name("std_string_move_assign_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_MoveAssign, hstring)->Arg(0)->Arg(1)->Name("hstring_move_assign_short")->Arg(0)->Name("hstring_move_assign_long")->Arg(1);

// 元素访问
template <typename StringType>
static void BM_OperatorIndex(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    size_t len = s.size();
    volatile char c;  // 防止优化
    for (auto _ : state) {
        for (size_t i = 0; i < len; ++i) {
            c = s[i];
            benchmark::DoNotOptimize(c);
        }
    }
}
BENCHMARK_TEMPLATE(BM_OperatorIndex, std::string)->Arg(0)->Arg(1)->Name("std_string_index_short")->Arg(0)->Name("std_string_index_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_OperatorIndex, hstring)->Arg(0)->Arg(1)->Name("hstring_index_short")->Arg(0)->Name("hstring_index_long")->Arg(1);

// 容量操作
template <typename StringType>
static void BM_Reserve(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    for (auto _ : state) {
        s.reserve(s.capacity() * 2);
        benchmark::DoNotOptimize(s);
    }
}
//BENCHMARK_TEMPLATE(BM_Reserve, std::string)->Arg(0)->Arg(1)->Name("std_string_reserve_short")->Arg(0)->Name("std_string_reserve_long")->Arg(1);
//BENCHMARK_TEMPLATE(BM_Reserve, hstring)->Arg(0)->Arg(1)->Name("hstring_reserve_short")->Arg(0)->Name("hstring_reserve_long")->Arg(1);

template <typename StringType>
static void BM_ShrinkToFit(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    s.reserve(s.capacity() * 2);  // 先扩容
    for (auto _ : state) {
        s.shrink_to_fit();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK_TEMPLATE(BM_ShrinkToFit, std::string)->Arg(0)->Arg(1)->Name("std_string_shrink_short")->Arg(0)->Name("std_string_shrink_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_ShrinkToFit, hstring)->Arg(0)->Arg(1)->Name("hstring_shrink_short")->Arg(0)->Name("hstring_shrink_long")->Arg(1);

// 修改操作
template <typename StringType>
static void BM_PushBack(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    for (auto _ : state) {
        s.push_back('a');
        benchmark::DoNotOptimize(s);
        s.pop_back();  // 恢复原状以便重复
    }
}
BENCHMARK_TEMPLATE(BM_PushBack, std::string)->Arg(0)->Arg(1)->Name("std_string_push_back_short")->Arg(0)->Name("std_string_push_back_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_PushBack, hstring)->Arg(0)->Arg(1)->Name("hstring_push_back_short")->Arg(0)->Name("hstring_push_back_long")->Arg(1);

template <typename StringType>
static void BM_Append(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    StringType suffix("_suffix");
    for (auto _ : state) {
        StringType copy = s;
        copy.append(suffix);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK_TEMPLATE(BM_Append, std::string)->Arg(0)->Arg(1)->Name("std_string_append_short")->Arg(0)->Name("std_string_append_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Append, hstring)->Arg(0)->Arg(1)->Name("hstring_append_short")->Arg(0)->Name("hstring_append_long")->Arg(1);

template <typename StringType>
static void BM_Insert(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    StringType ins(insert_payload);
    for (auto _ : state) {
        StringType copy = s;
        copy.insert(3, ins);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK_TEMPLATE(BM_Insert, std::string)->Arg(0)->Arg(1)->Name("std_string_insert_short")->Arg(0)->Name("std_string_insert_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Insert, hstring)->Arg(0)->Arg(1)->Name("hstring_insert_short")->Arg(0)->Name("hstring_insert_long")->Arg(1);

template <typename StringType>
static void BM_Erase(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    for (auto _ : state) {
        StringType copy = s;
        copy.erase(1, 2);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK_TEMPLATE(BM_Erase, std::string)->Arg(0)->Arg(1)->Name("std_string_erase_short")->Arg(0)->Name("std_string_erase_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Erase, hstring)->Arg(0)->Arg(1)->Name("hstring_erase_short")->Arg(0)->Name("hstring_erase_long")->Arg(1);

// 查找
template <typename StringType>
static void BM_Find(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    StringType pattern(find_pattern);
    for (auto _ : state) {
        auto pos = s.find(pattern);
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK_TEMPLATE(BM_Find, std::string)->Arg(0)->Arg(1)->Name("std_string_find_short")->Arg(0)->Name("std_string_find_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Find, hstring)->Arg(0)->Arg(1)->Name("hstring_find_short")->Arg(0)->Name("hstring_find_long")->Arg(1);

// 特别测试 hstring 的 KMP 查找
static void BM_MyStringFindKMP(benchmark::State& state) {
    hstring s(state.range(0) == 0 ? short_src : long_src);
    hstring pattern(find_pattern);
    for (auto _ : state) {
        auto pos = s.find_kmp(pattern);
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK(BM_MyStringFindKMP)->Arg(0)->Name("hstring_find_kmp_short")->Arg(0)->Name("hstring_find_kmp_long")->Arg(1);

// 比较
template <typename StringType>
static void BM_Compare(benchmark::State& state) {
    StringType s1(state.range(0) == 0 ? short_src : long_src);
    StringType s2(s1);
    for (auto _ : state) {
        auto res = s1.compare(s2);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_TEMPLATE(BM_Compare, std::string)->Arg(0)->Arg(1)->Name("std_string_compare_short")->Arg(0)->Name("std_string_compare_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Compare, hstring)->Arg(0)->Arg(1)->Name("hstring_compare_short")->Arg(0)->Name("hstring_compare_long")->Arg(1);

// 遍历迭代器
template <typename StringType>
static void BM_Iterate(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    volatile char c;
    for (auto _ : state) {
        for (auto it = s.begin(); it != s.end(); ++it) {
            c = *it;
            benchmark::DoNotOptimize(c);
        }
    }
}
BENCHMARK_TEMPLATE(BM_Iterate, std::string)->Arg(0)->Arg(1)->Name("std_string_iterate_short")->Arg(0)->Name("std_string_iterate_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Iterate, hstring)->Arg(0)->Arg(1)->Name("hstring_iterate_short")->Arg(0)->Name("hstring_iterate_long")->Arg(1);

// 子串
template <typename StringType>
static void BM_Substr(benchmark::State& state) {
    StringType s(state.range(0) == 0 ? short_src : long_src);
    for (auto _ : state) {
        auto sub = s.substr(1, 3);
        benchmark::DoNotOptimize(sub);
    }
}
BENCHMARK_TEMPLATE(BM_Substr, std::string)->Arg(0)->Arg(1)->Name("std_string_substr_short")->Arg(0)->Name("std_string_substr_long")->Arg(1);
BENCHMARK_TEMPLATE(BM_Substr, hstring)->Arg(0)->Arg(1)->Name("hstring_substr_short")->Arg(0)->Name("hstring_substr_long")->Arg(1);

// 分隔 (split)
static void BM_MyStringSplitChar(benchmark::State& state) {
    hstring s("a,b,c,d,e,f,g,h,i,j");
    for (auto _ : state) {
        auto tokens = s.split(',');
        benchmark::DoNotOptimize(tokens);
    }
}
BENCHMARK(BM_MyStringSplitChar);

static void BM_MyStringSplitString(benchmark::State& state) {
    hstring s("a=>b=>c=>d=>e=>f=>g=>h=>i=>j");
    hstring delim("=>");
    for (auto _ : state) {
        auto tokens = s.split(delim);
        benchmark::DoNotOptimize(tokens);
    }
}
BENCHMARK(BM_MyStringSplitString);

// 注意：std::string 没有直接 split 方法，此处不对比

// 主函数
BENCHMARK_MAIN();