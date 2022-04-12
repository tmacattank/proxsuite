#include <qp/proxqp/solver_sparse.hpp>
#include <qp/precond/ruiz.hpp>
#include <util.hpp>
#include <doctest.h>
#include <veg/util/dynstack_alloc.hpp>

using namespace qp;
using T = double;
using I = c_int;
using namespace sparse_ldlt::tags;

TEST_CASE("random ruiz") {

	for (auto const& dims : {
					 veg::tuplify(50, 0, 0),
					 veg::tuplify(50, 25, 0),
					 veg::tuplify(50, 0, 25),
					 veg::tuplify(50, 10, 25),
			 }) {
		VEG_BIND(auto const&, (n, n_eq, n_in), dims);

		double p = 1.0;

		auto H = ldlt_test::rand::sparse_positive_definite_rand(n, T(10.0), p);
		auto g = ldlt_test::rand::vector_rand<T>(n);
		auto AT = ldlt_test::rand::sparse_matrix_rand<T>(n, n_eq, p);
		auto b = ldlt_test::rand::vector_rand<T>(n_eq);
		auto CT = ldlt_test::rand::sparse_matrix_rand<T>(n, n_in, p);
		auto l = ldlt_test::rand::vector_rand<T>(n_in);
		auto u = (l.array() + 1).matrix().eval();

		{
			sparse::QpView<T, I> qp = {
					{sparse_ldlt::from_eigen, H},
					{sparse_ldlt::from_eigen, g},
					{sparse_ldlt::from_eigen, AT},
					{sparse_ldlt::from_eigen, b},
					{sparse_ldlt::from_eigen, CT},
					{sparse_ldlt::from_eigen, l},
					{sparse_ldlt::from_eigen, u},
			};

			qp::sparse::preconditioner::RuizEquilibration<T, I> ruiz{
					n,
					n_eq + n_in,
					1e-3,
					10,
					qp::sparse::preconditioner::Symmetry::UPPER,
			};

			sparse::QpWorkspace<T, I> work;
			QPSettings<T> settings;
			sparse::qp_setup(work, qp, ruiz);

			Eigen::Matrix<T, -1, 1> x(n);
			Eigen::Matrix<T, -1, 1> y(n_eq);
			Eigen::Matrix<T, -1, 1> z(n_in);
			x.setZero();
			y.setZero();
			z.setZero();

			sparse::qp_solve(
					{ldlt::from_eigen, x},
					{ldlt::from_eigen, y},
					{ldlt::from_eigen, z},
					work,
					settings,
					ruiz,
					qp);
			CHECK(
					infty_norm(
							H.selfadjointView<Eigen::Upper>() * x + g + AT * y + CT * z) <=
					1e-9);
			CHECK(infty_norm(AT.transpose() * x - b) <= 1e-9);
			if (n_in > 0) {
				CHECK((CT.transpose() * x - l).minCoeff() > -1e-9);
				CHECK((CT.transpose() * x - u).maxCoeff() < 1e-9);
			}
		}
	}
}

TEST_CASE("random id") {

	for (auto const& dims : {
					 veg::tuplify(50, 0, 0),
					 veg::tuplify(50, 25, 0),
					 veg::tuplify(50, 0, 25),
					 veg::tuplify(50, 10, 25),
			 }) {
		VEG_BIND(auto const&, (n, n_eq, n_in), dims);

		double p = 1.0;

		auto H = ldlt_test::rand::sparse_positive_definite_rand(n, T(10.0), p);
		auto g = ldlt_test::rand::vector_rand<T>(n);
		auto AT = ldlt_test::rand::sparse_matrix_rand<T>(n, n_eq, p);
		auto b = ldlt_test::rand::vector_rand<T>(n_eq);
		auto CT = ldlt_test::rand::sparse_matrix_rand<T>(n, n_in, p);
		auto l = ldlt_test::rand::vector_rand<T>(n_in);
		auto u = (l.array() + 1).matrix().eval();

		{
			sparse::QpView<T, I> qp = {
					{sparse_ldlt::from_eigen, H},
					{sparse_ldlt::from_eigen, g},
					{sparse_ldlt::from_eigen, AT},
					{sparse_ldlt::from_eigen, b},
					{sparse_ldlt::from_eigen, CT},
					{sparse_ldlt::from_eigen, l},
					{sparse_ldlt::from_eigen, u},
			};

			qp::sparse::preconditioner::Identity<T, I> id;

			sparse::QpWorkspace<T, I> work;
			QPSettings<T> settings;
			sparse::qp_setup(work, qp, id);

			Eigen::Matrix<T, -1, 1> x(n);
			Eigen::Matrix<T, -1, 1> y(n_eq);
			Eigen::Matrix<T, -1, 1> z(n_in);
			x.setZero();
			y.setZero();
			z.setZero();

			sparse::qp_solve(
					{ldlt::from_eigen, x},
					{ldlt::from_eigen, y},
					{ldlt::from_eigen, z},
					work,
					settings,
					id,
					qp);
			CHECK(
					infty_norm(
							H.selfadjointView<Eigen::Upper>() * x + g + AT * y + CT * z) <=
					1e-9);
			CHECK(infty_norm(AT.transpose() * x - b) <= 1e-9);
			if (n_in > 0) {
				CHECK((CT.transpose() * x - l).minCoeff() > -1e-9);
				CHECK((CT.transpose() * x - u).maxCoeff() < 1e-9);
			}
		}
	}
}