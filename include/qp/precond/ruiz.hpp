#ifndef INRIA_LDLT_RUIZ_HPP_XXCS7AMRS
#define INRIA_LDLT_RUIZ_HPP_XXCS7AMRS

#include "ldlt/detail/tags.hpp"
#include "ldlt/detail/macros.hpp"
#include "ldlt/detail/simd.hpp"
#include "ldlt/views.hpp"
#include "ldlt/detail/meta.hpp"
#include "qp/views.hpp"
#include <ostream>

#include <Eigen/Core>

namespace qp {
enum struct Symmetry {
	general,
	lower,
	upper,
};

namespace detail {
namespace nb {
struct sqrt {
	template <typename Scalar>
	auto operator()(Scalar x) const -> Scalar {
		using std::sqrt;
		return sqrt(x);
	}
};
} // namespace nb
LDLT_DEFINE_NIEBLOID(sqrt);

template <typename Scalar, Layout LH, Layout LC>
auto ruiz_scale_qp_in_place( //
		VectorViewMut<Scalar> delta_,
		std::ostream* logger_ptr,
		qp::QpViewMut<Scalar, LH, LC> qp,
		Scalar epsilon,
		i32 max_iter,
		Symmetry sym) -> Scalar {

	Scalar c(1);
	auto S = to_eigen_vector_mut(delta_);

	auto H = to_eigen_matrix_mut(qp.H);
	auto g = to_eigen_vector_mut(qp.g);
	auto A = to_eigen_matrix_mut(qp.A);
	auto b = to_eigen_vector_mut(qp.b);
	auto C = to_eigen_matrix_mut(qp.C);
	auto d = to_eigen_vector_mut(qp.d);

	Scalar machine_eps = std::numeric_limits<Scalar>::epsilon();
	/*
	 * compute equilibration parameters and scale in place the qp following
	 * algorithm 1 at
	 * https://sharelatex.irisa.fr/project/6100343e095d2649760f5631
	 */

	i32 n = qp.H.rows;
	i32 n_eq = qp.A.rows;
	i32 n_in = qp.C.rows;

	S.setConstant(Scalar(1));
	Scalar gamma = Scalar(1);

	LDLT_WORKSPACE_MEMORY(_delta, n + n_eq + n_in, Scalar);
	auto delta = to_eigen_vector_mut(VectorViewMut<Scalar>{
			_delta,
			n + n_eq + n_in,
	});

	delta.setZero();

	i32 iter = 1;
	while (infty_norm((1 - delta.array()).matrix()) > epsilon) {
		if (iter >= max_iter) {
			if (logger_ptr != nullptr) {
				*logger_ptr                                     //
						<< "j : "                                   //
						<< iter                                     //
						<< " ; error : "                            //
						<< infty_norm((1 - delta.array()).matrix()) //
						<< "\n\n";
			}
			break;
		} else {
			if (logger_ptr != nullptr) {
				*logger_ptr                                     //
						<< "j : "                                   //
						<< iter                                     //
						<< " ; error : "                            //
						<< infty_norm((1 - delta.array()).matrix()) //
						<< "\n\n";
			}
			iter += 1;
		}

		// normalization vector
		{
			LDLT_DECL_SCOPE_TIMER("ruiz equilibration", "delta computation", Scalar);
			for (i32 k = 0; k < n; ++k) {
				switch (sym) {
				case Symmetry::upper: { // upper triangular part
					delta(k) = Scalar(1) / (sqrt(max2(
																			infty_norm(H.row(k).tail(n - k)), //
																			max2(                             //
																					infty_norm(A.col(k)),
																					infty_norm(C.col(k))))) +
					                        machine_eps);
					break;
				}
				case Symmetry::lower: { // lower triangular part
					delta(k) = Scalar(1) / (sqrt(max2(
																			infty_norm(H.col(k).tail(n - k)), //
																			max2(                             //
																					infty_norm(A.col(k)),
																					infty_norm(C.col(k))))) +
					                        machine_eps);
					break;
				}
				case Symmetry::general: {
					delta(k) = Scalar(1) / (sqrt(max2(
																			infty_norm(H.col(k)), //
																			max2(                 //
																					infty_norm(A.col(k)),
																					infty_norm(C.col(k))))) +
					                        machine_eps);
					break;
				}
				default: {
				}
				}
			}

			for (i32 k = 0; k < n_eq; ++k) {
				delta(n + k) = Scalar(1) / (sqrt(infty_norm(A.row(k))) + machine_eps);
			}
			for (i32 k = 0; k < n_in; ++k) {
				delta(k + n + n_eq) =
						Scalar(1) / (sqrt(infty_norm(C.row(k))) + machine_eps);
			}
		}

		{
			LDLT_DECL_SCOPE_TIMER("ruiz equilibration", "normalization", Scalar);
			// normalize H
			switch (sym) {
			case Symmetry::upper: {
				// upper triangular part
				for (i32 j = 0; j < n; ++j) {
					H.col(j).head(j + 1) *= delta(j);
				}
				// normalisation des lignes
				for (i32 i = 0; i < n; ++i) {
					H.row(i).tail(n - i) *= delta(i);
				}
				break;
			}
			case Symmetry::lower: {
				// lower triangular part
				for (i32 j = 0; j < n; ++j) {
					H.col(j).tail(n - j) *= delta(j);
				}
				// normalisation des lignes
				for (i32 i = 0; i < n; ++i) {
					H.row(i).head(i + 1) *= delta(i);
				}
				break;
			}
			case Symmetry::general: {
				// all matrix
				H = delta.head(n).asDiagonal() * H * delta.head(n).asDiagonal();
				break;
			}
			default: {
			}
			}
			// normalize A and C
			A = delta.middleRows(n, n_eq).asDiagonal() * A *
			    delta.head(n).asDiagonal();
			C = delta.tail(n_in).asDiagonal() * C * delta.head(n).asDiagonal();
			// normalize vectors
			g.array() *= delta.head(n).array();
			b.array() *= delta.middleRows(n, n_eq).array();
			d.array() *= delta.tail(n_in).array();
			// additional normalization for the cost function

			switch (sym) {
			case Symmetry::upper: {
				// upper triangular part
				Scalar tmp = Scalar(0) ; 
				for (i32 j = 0; j < n; ++j) {
					tmp += infty_norm(H.row(j).tail(n-j));
				}
			 	gamma = 1 /  max2(tmp/n,max2(infty_norm(g), Scalar(1)))  ;
				break;
			}
			case Symmetry::lower: {
				// lower triangular part
				Scalar tmp = Scalar(0); 
				for (i32 j = 0; j < n; ++j) {
					tmp += infty_norm(H.col(j).tail(n - j));
				}
				gamma = 1 / max2(tmp/n,max2(infty_norm(g), Scalar(1))) ;
				break;
			}
			case Symmetry::general: {
				// all matrix
				gamma =
					1 / max2(
							max2(infty_norm(g), Scalar(1)),
							(H.colwise().template lpNorm<Eigen::Infinity>()).mean());
				break;
			}
			default: {
			}
			}
			g *= gamma;
			H *= gamma;

			S.array() *= delta.array(); // coefficientwise product
			c *= gamma;
		}
	}
	return c;
}
} // namespace detail

namespace preconditioner {
template <typename T>
using Vec = Eigen::Matrix<T, Eigen::Dynamic, 1>;

template <typename Scalar, Layout LH, Layout LC>
struct RuizEquilibration {
	Vec<Scalar> delta;
	Scalar c;
	i32 dim;
	Scalar epsilon;
	i32 max_iter;
	Symmetry sym;

	std::ostream* logger_ptr = nullptr;

	explicit RuizEquilibration(
			i32 dim_,
			i32 n_eq_in,
			Scalar epsilon_ = Scalar(1e-3),
			i32 max_iter_ = 20,
			Symmetry sym_ = Symmetry::general,
			std::ostream* logger = nullptr)
			: delta(dim_ + n_eq_in),
				c(1),
				dim(dim_),
				epsilon(epsilon_),
				max_iter(max_iter_),
				sym(sym_),
				logger_ptr(logger) {
		delta.setZero();
	}
	void print() {
		// CHANGE: endl to newline
		*logger_ptr << " delta : " << delta << "\n\n";
		*logger_ptr << " c : " << c << "\n\n";
	}

	// H_new = c * head @ H @ head
	// A_new = tail @ A @ head
	// g_new = c * head @ g
	// b_new = tail @ b
	void scale_qp_in_place(QpViewMut<Scalar, LH, LC> qp) {
		c = detail::ruiz_scale_qp_in_place(
				detail::from_eigen_vector_mut(delta),
				logger_ptr,
				qp,
				epsilon,
				max_iter,
				sym);
	}
	void
	scale_qp(QpView<Scalar, LH, LC> qp, QpViewMut<Scalar, LH, LC> scaled_qp) {

		using namespace detail;
		/*
		 * scaled_qp is scaled, whereas first qp is not
		 * the procedure computes as well equilibration parameters using default
		 * parameters following algorithm 1 at
		 * https://sharelatex.irisa.fr/project/6100343e095d2649760f5631
		 */

		to_eigen_matrix_mut(scaled_qp.H) = to_eigen_matrix(qp.H);
		to_eigen_matrix_mut(scaled_qp.A) = to_eigen_matrix(qp.A);
		to_eigen_matrix_mut(scaled_qp.C) = to_eigen_matrix(qp.C);
		to_eigen_vector_mut(scaled_qp.g) = to_eigen_vector(qp.g);
		to_eigen_vector_mut(scaled_qp.b) = to_eigen_vector(qp.b);
		to_eigen_vector_mut(scaled_qp.d) = to_eigen_vector(qp.d);

		scale_qp_in_place(scaled_qp, epsilon, max_iter);
	}
	// modifies variables in place
	void scale_primal_in_place(VectorViewMut<Scalar> primal) {
		detail::to_eigen_vector_mut(primal).array() /= delta.array().head(dim);
	}
	void scale_dual_in_place(VectorViewMut<Scalar> dual) {
		detail::to_eigen_vector_mut(dual).array() =
				detail::to_eigen_vector(dual.as_const()).array() /
				delta.tail(delta.size() - dim).array() * c;
	}
	void unscale_primal_in_place(VectorViewMut<Scalar> primal) {
		detail::to_eigen_vector_mut(primal).array() *= delta.array().head(dim);
	}
	void unscale_dual_in_place(VectorViewMut<Scalar> dual) {
		detail::to_eigen_vector_mut(dual).array() =
				detail::to_eigen_vector(dual.as_const()).array() *
				delta.tail(delta.size() - dim).array() / c;
	}

	// modifies residuals in place
	void scale_primal_residual_in_place(VectorViewMut<Scalar> primal) {
		detail::to_eigen_vector_mut(primal).array() *=
				delta.tail(delta.size() - dim).array();
	}
	void scale_dual_residual_in_place(VectorViewMut<Scalar> dual) {
		detail::to_eigen_vector_mut(dual).array() *= delta.head(dim).array() * c;
	}
	void unscale_primal_residual_in_place(VectorViewMut<Scalar> primal) {
		detail::to_eigen_vector_mut(primal).array() /=
				delta.tail(delta.size() - dim).array();
	}
	void unscale_dual_residual_in_place(VectorViewMut<Scalar> dual) {
		detail::to_eigen_vector_mut(dual).array() /= delta.head(dim).array() * c;
	}
};

} // namespace preconditioner

} // namespace qp

#endif /* end of include guard INRIA_LDLT_RUIZ_HPP_XXCS7AMRS */