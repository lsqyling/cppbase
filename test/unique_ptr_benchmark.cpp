#include <benchmark/benchmark.h>
#include "mystl/unique_ptr.hpp"
#include <memory>
#include <vector>

// ==================== 辅助类型 ====================

struct HeavyObject {
    int data[64];  // 256 bytes
    HeavyObject() { for (int i = 0; i < 64; ++i) data[i] = i; }
    virtual ~HeavyObject() = default;
    virtual int sum() const {
        int s = 0;
        for (int i = 0; i < 64; ++i) s += data[i];
        return s;
    }
};

struct EmptyDeleter {
    void operator()(HeavyObject*) const noexcept {}
};

struct StatefulDeleter {
    int state = 0;
    void operator()(HeavyObject* p) const { delete p; }
};

// ==================== 构造与析构 Benchmark ====================

static void BM_MyUniquePtr_NewDelete(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyUniquePtr_NewDelete);

static void BM_StdUniquePtr_NewDelete(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject> p(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_StdUniquePtr_NewDelete);

// ==================== 移动构造 Benchmark ====================

static void BM_MyUniquePtr_MoveConstruct(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p1(new HeavyObject);
        my::unique_ptr<HeavyObject> p2(std::move(p1));
        benchmark::DoNotOptimize(p2);
    }
}
BENCHMARK(BM_MyUniquePtr_MoveConstruct);

static void BM_StdUniquePtr_MoveConstruct(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject> p1(new HeavyObject);
        std::unique_ptr<HeavyObject> p2(std::move(p1));
        benchmark::DoNotOptimize(p2);
    }
}
BENCHMARK(BM_StdUniquePtr_MoveConstruct);

// ==================== 移动赋值 Benchmark ====================

static void BM_MyUniquePtr_MoveAssign(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p1(new HeavyObject);
        my::unique_ptr<HeavyObject> p2(new HeavyObject);
        p2 = std::move(p1);
        benchmark::DoNotOptimize(p2);
    }
}
BENCHMARK(BM_MyUniquePtr_MoveAssign);

static void BM_StdUniquePtr_MoveAssign(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject> p1(new HeavyObject);
        std::unique_ptr<HeavyObject> p2(new HeavyObject);
        p2 = std::move(p1);
        benchmark::DoNotOptimize(p2);
    }
}
BENCHMARK(BM_StdUniquePtr_MoveAssign);

// ==================== Reset / Release Benchmark ====================

static void BM_MyUniquePtr_Reset(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p(new HeavyObject);
        p.reset(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyUniquePtr_Reset);

static void BM_StdUniquePtr_Reset(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject> p(new HeavyObject);
        p.reset(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_StdUniquePtr_Reset);

// ==================== Release benchmark ====================

static void BM_MyUniquePtr_Release(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p(new HeavyObject);
        HeavyObject* raw = p.release();
        delete raw;
    }
}
BENCHMARK(BM_MyUniquePtr_Release);

static void BM_StdUniquePtr_Release(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject> p(new HeavyObject);
        HeavyObject* raw = p.release();
        delete raw;
    }
}
BENCHMARK(BM_StdUniquePtr_Release);

// ==================== EBO 影响测试 ====================

static void BM_MyUniquePtr_StatefulDeleter(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject, StatefulDeleter> p(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyUniquePtr_StatefulDeleter);

static void BM_MyUniquePtr_EmptyDeleter(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject, EmptyDeleter> p(new HeavyObject);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyUniquePtr_EmptyDeleter);

// ==================== 数组版本 Benchmark ====================

static void BM_MyUniquePtr_ArrayNewDelete(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject[]> p(new HeavyObject[16]);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyUniquePtr_ArrayNewDelete);

static void BM_StdUniquePtr_ArrayNewDelete(benchmark::State& state) {
    for (auto _ : state) {
        std::unique_ptr<HeavyObject[]> p(new HeavyObject[16]);
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_StdUniquePtr_ArrayNewDelete);

// ==================== 批量操作 Benchmark ====================

static void BM_MyUniquePtr_VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<my::unique_ptr<int>> vec;
        vec.reserve(1024);
        for (int i = 0; i < 1024; ++i) {
            vec.push_back(my::unique_ptr<int>(new int(i)));
        }
        benchmark::DoNotOptimize(vec);
    }
}
BENCHMARK(BM_MyUniquePtr_VectorPushBack);

static void BM_StdUniquePtr_VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::unique_ptr<int>> vec;
        vec.reserve(1024);
        for (int i = 0; i < 1024; ++i) {
            vec.push_back(std::unique_ptr<int>(new int(i)));
        }
        benchmark::DoNotOptimize(vec);
    }
}
BENCHMARK(BM_StdUniquePtr_VectorPushBack);

// ==================== make_uniqueptr vs make_unique ====================

static void BM_MyMakeUniquePtr(benchmark::State& state) {
    for (auto _ : state) {
        auto p = my::make_uniqueptr<HeavyObject>();
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_MyMakeUniquePtr);

static void BM_StdMakeUnique(benchmark::State& state) {
    for (auto _ : state) {
        auto p = std::make_unique<HeavyObject>();
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_StdMakeUnique);

// ==================== 转换移动构造 Benchmark ====================

static void BM_MyUniquePtr_ConvertMove(benchmark::State& state) {
    for (auto _ : state) {
        my::unique_ptr<HeavyObject> p(new HeavyObject);
        my::unique_ptr<HeavyObject> p2(std::move(p));
        benchmark::DoNotOptimize(p2);
    }
}
BENCHMARK(BM_MyUniquePtr_ConvertMove);

BENCHMARK_MAIN();
