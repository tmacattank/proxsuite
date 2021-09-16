#include <benchmark/benchmark.h>
#include <Eigen/Cholesky>
#include <ldlt/factorize.hpp>
#include <util.hpp>

#define LDLT_BENCHMARK_MAIN BENCHMARK_MAIN    /* NOLINT */
#define LDLT_BENCHMARK BENCHMARK              /* NOLINT */
#define LDLT_BENCHMARK_TPL BENCHMARK_TEMPLATE /* NOLINT */

using namespace ldlt;

template <typename T, Layout L>
using Mat = Eigen::
		Matrix<T, -1, -1, (L == colmajor) ? Eigen::ColMajor : Eigen::RowMajor>;
template <typename T>
using Vec = Eigen::Matrix<T, -1, 1>;

template <typename T, Layout L>
void bench_eigen__ldlt(benchmark::State& s) {
	isize dim = isize(s.range(0));

	ldlt_test::rand::set_seed(0);
	Mat<T, L> a = ldlt_test::rand::positive_definite_rand<T>(dim, T(1e2));
	Eigen::LDLT<Mat<T, L>> l(a);

	benchmark::DoNotOptimize(a.data());
	benchmark::DoNotOptimize(l.matrixLDLT().data());

	for (auto _ : s) {
		l.compute(a);
		benchmark::ClobberMemory();
	}
}

template <typename T, Layout L>
void bench_eigen__chol(benchmark::State& s) {
	isize dim = isize(s.range(0));

	ldlt_test::rand::set_seed(0);
	Mat<T, L> a = ldlt_test::rand::positive_definite_rand<T>(dim, T(1e2));
	Eigen::LLT<Mat<T, L>> l(a);

	benchmark::DoNotOptimize(a.data());
	benchmark::DoNotOptimize(l.matrixLLT().data());

	for (auto _ : s) {
		l.compute(a);
		benchmark::ClobberMemory();
	}
}

template <typename T, Layout L>
void bench_ldlt_______(benchmark::State& s) {

	isize block_size = isize(s.range(0));
	isize dim = isize(s.range(1));
	ldlt_test::rand::set_seed(0);
	Mat<T, L> a = ldlt_test::rand::positive_definite_rand<T>(dim, T(1e2));

	Mat<T, colmajor> l(dim, dim);
	l.setZero();
	Vec<T> d(dim);
	d.setZero();

	benchmark::DoNotOptimize(a.data());
	benchmark::DoNotOptimize(l.data());
	benchmark::DoNotOptimize(d.data());

	for (auto _ : s) {
		auto a_view = MatrixView<T, L>{from_eigen, a};
		auto ldl_view = ldlt::LdltViewMut<T>{
				{from_eigen, l},
				{from_eigen, d},
		};

		factorize(ldl_view, a_view, factorization_strategy::blocked(block_size));
		benchmark::ClobberMemory();
	}
}

template <typename T>
void bench_inplace____(benchmark::State& s) {

	isize block_size = isize(s.range(0));
	isize dim = isize(s.range(1));
	ldlt_test::rand::set_seed(0);
	Mat<T, colmajor> a = ldlt_test::rand::positive_definite_rand<T>(dim, T(1e2));

	Mat<T, colmajor> l(dim, dim);
	l.setZero();
	Vec<T> d(dim);
	d.setZero();

	benchmark::DoNotOptimize(a.data());
	benchmark::DoNotOptimize(l.data());
	benchmark::DoNotOptimize(d.data());

	for (auto _ : s) {
		auto a_view = MatrixView<T, colmajor>{from_eigen, a};
		auto ldl_view = ldlt::LdltViewMut<T>{
				{from_eigen, l},
				{from_eigen, d},
		};

		l = a;
		factorize(ldl_view, a_view, factorization_strategy::blocked(block_size));
		benchmark::ClobberMemory();
	}
}

template <typename T>
void bench_inplace_alt(benchmark::State& s) {

	isize block_size = isize(s.range(0));
	isize dim = isize(s.range(1));
	ldlt_test::rand::set_seed(0);
	Mat<T, colmajor> a = ldlt_test::rand::positive_definite_rand<T>(dim, T(1e2));

	Mat<T, colmajor> l(dim, dim);
	l.setZero();
	Vec<T> d(dim);
	d.setZero();
	Mat<T, colmajor> work(dim, block_size);

	benchmark::DoNotOptimize(a.data());
	benchmark::DoNotOptimize(l.data());
	benchmark::DoNotOptimize(d.data());

	for (auto _ : s) {
		auto a_view = MatrixView<T, colmajor>{from_eigen, a};
		auto ldl_view = ldlt::LdltViewMut<T>{
				{from_eigen, l},
				{from_eigen, d},
		};

		l = a;
		factorize(
				ldl_view, a_view, factorization_strategy::alt_blocked(block_size));
		benchmark::ClobberMemory();
	}
}

void bench_dummy(benchmark::State& s) {
	for (auto _ : s) {
	}
}

LDLT_BENCHMARK_MAIN();

constexpr isize dim_tiny = 32;
constexpr isize dim_small = 128;
constexpr isize dim_medium = 512;
constexpr isize dim_large = 1024;
constexpr isize dim_huge = 4096;

#define LDLT_BENCH(Dim, Type, L)                                               \
	LDLT_BENCHMARK(bench_dummy);                                                 \
	LDLT_BENCHMARK_TPL(bench_eigen__chol, Type, L)->Arg(Dim);                    \
	LDLT_BENCHMARK_TPL(bench_eigen__ldlt, Type, L)->Arg(Dim);                    \
	LDLT_BENCHMARK_TPL(bench_ldlt_______, Type, L)->Args({1, Dim});              \
	LDLT_BENCHMARK_TPL(bench_ldlt_______, Type, L)->Args({16, Dim});             \
	LDLT_BENCHMARK_TPL(bench_ldlt_______, Type, L)->Args({32, Dim});             \
	LDLT_BENCHMARK_TPL(bench_ldlt_______, Type, L)->Args({64, Dim});             \
	LDLT_BENCHMARK_TPL(bench_ldlt_______, Type, L)->Args({128, Dim});

#define LDLT_BENCH_LAYOUT(Dim, Type)                                           \
	LDLT_BENCHMARK(bench_dummy);                                                 \
	LDLT_BENCH(Dim, Type, colmajor);                                             \
	LDLT_BENCH(Dim, Type, rowmajor);                                             \
	LDLT_BENCHMARK_TPL(bench_inplace____, Type)->Args({1, Dim});                 \
	LDLT_BENCHMARK_TPL(bench_inplace____, Type)->Args({16, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace____, Type)->Args({32, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace____, Type)->Args({64, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace____, Type)->Args({128, Dim});               \
	LDLT_BENCHMARK_TPL(bench_inplace_alt, Type)->Args({1, Dim});                 \
	LDLT_BENCHMARK_TPL(bench_inplace_alt, Type)->Args({16, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace_alt, Type)->Args({32, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace_alt, Type)->Args({64, Dim});                \
	LDLT_BENCHMARK_TPL(bench_inplace_alt, Type)->Args({128, Dim});

#define LDLT_BENCH_DIM(Type)                                                   \
	LDLT_BENCH_LAYOUT(dim_tiny, Type);                                           \
	LDLT_BENCH_LAYOUT(dim_small, Type);                                          \
	LDLT_BENCH_LAYOUT(dim_medium, Type);                                         \
	LDLT_BENCH_LAYOUT(dim_large, Type);                                          \
	LDLT_BENCH_LAYOUT(dim_huge, Type)

LDLT_BENCH_DIM(f64);
LDLT_BENCH_DIM(f32);
