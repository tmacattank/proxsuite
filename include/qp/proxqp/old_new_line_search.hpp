#ifndef INRIA_LDLT_OLD_NEW_LINE_SEARCH_HPP_2TUXO5DFS
#define INRIA_LDLT_OLD_NEW_LINE_SEARCH_HPP_2TUXO5DFS

#include "ldlt/views.hpp"
#include "qp/views.hpp"
#include <cmath>
#include <iostream>
#include <fstream>
#include <list>

namespace qp {
inline namespace tags {
using namespace ldlt::tags;
}
namespace line_search {


template <typename T, Layout LC>
auto oldNew_gradient_norm_computation_box(
		qp::Qpdata<T>& qpmodel,
		VectorView<T> ze,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dz,
		T mu_in_inv,
		MatrixView<T, LC> C,
		VectorView<T> Cdx_,
		VectorView<T> residual_in_z_u_,
		VectorView<T> residual_in_z_l_,
		VectorView<T> d_dual_for_eq_,
		VectorView<T> dual_for_eq_,
		VectorView<T> d_primal_residual_eq_,
		VectorView<T> primal_residual_eq_,
		T alpha,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& active_part_z,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& res,

		VectorViewMut<isize> _active_set_l,
		VectorViewMut<isize> _active_set_u,
		VectorViewMut<isize> _inactive_set

		) -> T {

	/*
	 * Compute the squared norm of the following vector res
	 *
	 * vect1 = H.dot(x) + g + rho_primal_proximal * (x-xe)
	 *       + A.transpose().dot(y)
	 *       + C[active_inequalities_l,:].T × z[active_inequalities_l]
	 *       + C[active_inequalities_u,:].T × z[active_inequalities_u]
	 *
	 * vect3_u = residual_in_u[active_inequalities_u]
	 *         - (z[active_inequalities_u] - ze[active_inequalities_u])/mu_in
	 * vect3_l = residual_in_l[active_inequalities_l]
	 *         - (z[active_inequalities_l]-ze[active_inequalities_l])/mu_in
	 * vect4 = z[inactive_inequalities]
	 *
	 * res = np.concatenate((
	 *         vect1,
	 *         (residual_eq * mu_eq - (y-ye))/mu_eq,
	 *         vect3_u,
	 *         vect3_l,
	 *         vect4,
	 *       ), axis = None)
	 *
	 * considering the following qp problem : (H, g, A, b, C, u,l) and
	 *
	 * residual_eq = A.dot(x) - b
	 * residual_in_u = C.dot(x) - u
	 * residual_in_l = C.dot(x) - l
	 * active_inequalities_u = residual_in_u + z/mu_in >= 0
	 * active_inequalities_u = residual_in_l + z/mu_in <= 0
	 * active_inequalities = active_inequalities_u + active_inequalities_l
	 * inactive_inequalities = ~active_inequalities
	 */

	auto C_copy = C.to_eigen();
	auto z_e = ze.to_eigen();

	auto Cdx = Cdx_.to_eigen();
	auto residual_in_z_u = residual_in_z_u_.to_eigen();
	auto residual_in_z_l = residual_in_z_l_.to_eigen();
	auto d_dual_for_eq = d_dual_for_eq_.to_eigen();
	auto dual_for_eq = dual_for_eq_.to_eigen();
	auto d_primal_residual_eq = d_primal_residual_eq_.to_eigen();
	auto primal_residual_eq = primal_residual_eq_.to_eigen();

	active_part_z.setZero();
	aux_u.setZero();
	aux_l.setZero();
	res.setZero();
	// define active set
	tmp_u.noalias() = residual_in_z_u + alpha * Cdx;
	tmp_l.noalias() = residual_in_z_l + alpha * Cdx;
	
	isize num_active_u = 0;
	isize num_active_l = 0;
	isize num_inactive = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) >= T(0.)) {
			num_active_u += 1;
		}
		if (tmp_l(k) <= T(0.)) {
			num_active_l += 1;
		}
		if (tmp_u(k) < T(0.) && tmp_l(k) > T(0.)) {
			num_inactive += 1;
		}
	}

	auto active_set_u = _active_set_u.to_eigen();
	auto active_set_l = _active_set_l.to_eigen();
	auto inactive_set = _inactive_set.to_eigen();

	inactive_set.setZero();
	active_set_u.setZero();
	isize i_u = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) >= T(0.)) {
			active_set_u(i_u) = k;
			i_u += 1;
		}
	}

	active_set_l.setZero();
	isize i_l = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_l(k) <= T(0.)) {
			active_set_l(i_l) = k;
			i_l += 1;
		}
	}

	isize i_inact = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) < T(0.) && tmp_l(k) > T(0.)) {
			inactive_set(i_inact) = k;
			i_inact += 1;
		}
	}

	// form the gradient
	active_part_z.noalias() = z_e + alpha * dz;
	for (isize k = 0; k < num_active_u; ++k) {
		if (active_part_z(active_set_u(k)) < T(0.)) {
			active_part_z(active_set_u(k)) = T(0.);
		}
	}
	for (isize k = 0; k < num_active_l; ++k) {
		if (active_part_z(active_set_l(k)) > T(0.)) {
			active_part_z(active_set_l(k)) = T(0.);
		}
	}

	res.setZero();

	res.topRows(qpmodel._dim).noalias() = dual_for_eq + alpha * d_dual_for_eq;
	aux_u.setZero();
	for (isize k = 0; k < num_active_u; ++k) {
		res.topRows(qpmodel._dim).noalias() +=
				active_part_z(active_set_u(k)) * C_copy.row(active_set_u(k));
		aux_u.noalias()+= active_part_z(active_set_u(k)) * C_copy.row(active_set_u(k));
	}

	aux_l.setZero();
	for (isize k = 0; k < num_active_l; ++k) {
		res.topRows(qpmodel._dim).noalias() +=
				active_part_z(active_set_l(k)) * C_copy.row(active_set_l(k));
		aux_l.noalias() += active_part_z(active_set_l(k)) * C_copy.row(active_set_l(k));
	}

	res.middleRows(qpmodel._dim, qpmodel._n_eq).noalias() = primal_residual_eq + alpha * d_primal_residual_eq;
	for (isize k = 0; k < num_active_u; ++k) {
		res(qpmodel._dim + qpmodel._n_eq + k) =
				tmp_u(active_set_u(k)) - active_part_z(active_set_u(k)) * mu_in_inv;
	}
	for (isize k = 0; k < num_active_l; ++k) {
		res(qpmodel._dim + qpmodel._n_eq + num_active_u + k) =
				tmp_l(active_set_l(k)) - active_part_z(active_set_l(k)) * mu_in_inv;
	}
	for (isize k = 0; k < num_inactive; ++k) {
		res(qpmodel._dim + qpmodel._n_eq + num_active_u + num_active_l + k) =
				active_part_z(inactive_set(k));
	}

	return res.squaredNorm();
}

template <typename T>
auto oldNew_gradient_norm_qpalm_box(
		qp::Qpdata<T>& qpmodel,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> dx,
		T mu_eq,
		T mu_in,
		T rho,
		T alpha,
		VectorView<T> Hdx_,
		VectorView<T> g_,
		VectorView<T> Adx_,
		VectorView<T> residual_in_y_,
		VectorView<T> residual_in_z_u_,
		VectorView<T> residual_in_z_l_,
		VectorView<T> Cdx_,
		
		VectorViewMut<T> _tmp_u,
		VectorViewMut<T> _tmp_l,
		VectorViewMut<isize> _active_set_u,
		VectorViewMut<isize> _active_set_l,

		VectorViewMut<T> _tmp_a0_u,
		VectorViewMut<T> _tmp_b0_u,
		VectorViewMut<T> _tmp_a0_l,
		VectorViewMut<T> _tmp_b0_l,

		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_u,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_l,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_u

		) -> T {

	/*
	 * the function computes the first derivative of the proximal augmented
	 * lagrangian of the problem
	 *
	 * phi(alpha) = f(x_k+alpha dx) + rho/2 |x_k + alpha dx - x_k|**2
	 *              + mu_eq/2 (|A(x_k+alpha dx)-d+y_k/mu_eq|**2 - |y_k/mu_eq|**2)
	 *              + mu_in/2 ( | [C(x_k+alpha dx) - u + z_k/mu_in]_+ |**2
	 *                         +| [C(x_k+alpha dx) - l + z_k/mu_in]_- |**2
	 *                         - |z_k / mu_in|**2 )
	 * with f(x) = 0.5 * x^THx + g^Tx
	 * phi is a second order polynomial in alpha.
	 * Below are computed its coefficient a0 and b0
	 * in order to compute the desired gradient a0 * alpha + b0
	 */

	auto x_ = x.to_eigen();
	auto xe_ = xe.to_eigen();
	auto dx_ = dx.to_eigen();

	auto Cdx = Cdx_.to_eigen();
	auto Hdx = Hdx_.to_eigen();
	auto Adx = Adx_.to_eigen();
	auto g = g_.to_eigen();
	auto residual_in_y = residual_in_y_.to_eigen();
	auto residual_in_z_u = residual_in_z_u_.to_eigen();
	auto residual_in_z_l = residual_in_z_l_.to_eigen();

	// define active set

	auto tmp_u = _tmp_u.to_eigen();
	auto tmp_l = _tmp_l.to_eigen();

	tmp_u.noalias() = residual_in_z_u + Cdx * alpha;
	tmp_l.noalias() = residual_in_z_l + Cdx * alpha;
	l_active_set_n_u = tmp_u.array() > 0;
	l_active_set_n_l = tmp_l.array() < 0;

	isize num_active_u = 0;
	isize num_active_l = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (l_active_set_n_u(k)) {
			num_active_u += 1;
		}
		if (l_active_set_n_l(k)) {
			num_active_l += 1;
		}
	}

	auto active_set_u = _active_set_u.to_eigen();
	auto active_set_l = _active_set_l.to_eigen();
	active_set_u.setZero();
	active_set_l.setZero();

	isize i = 0;
	isize j = 0;

	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (l_active_set_n_u(k)) {
			active_set_u(i) = k;
			i += 1;
		}
		if (l_active_set_n_l(k)) {
			active_set_l(j) = k;
			j += 1;
		}
	}

	// coefficient computation

	auto tmp_a0_u = _tmp_a0_u.to_eigen();
	tmp_a0_u.setZero();
	auto tmp_b0_u = _tmp_b0_u.to_eigen();
	tmp_b0_u.setZero();

	auto tmp_a0_l = _tmp_a0_l.to_eigen();
	tmp_a0_l.setZero();
	auto tmp_b0_l = _tmp_b0_l.to_eigen();
	tmp_b0_l.setZero();

	for (isize k = 0; k < num_active_u; ++k) {
		tmp_a0_u(k) = Cdx(active_set_u(k));
		tmp_b0_u(k) = residual_in_z_u(active_set_u(k));
	}
	for (isize k = 0; k < num_active_l; ++k) {
		tmp_a0_l(k) = Cdx(active_set_l(k));
		tmp_b0_l(k) = residual_in_z_l(active_set_l(k));
	}
	for (isize k = 0; k < num_active_l; ++k) {
		tmp_a0_l(k) = Cdx(active_set_l(k));
		tmp_b0_l(k) = residual_in_z_l(active_set_l(k));
	}

	T a = dx_.dot(Hdx) + mu_eq * (Adx).squaredNorm() +
	      mu_in * (tmp_a0_u.squaredNorm() + tmp_a0_l.squaredNorm()) +
	      rho * dx_.squaredNorm();

	aux_u = rho * (x_ - xe_) + g;
	T b = x_.dot(Hdx) + (aux_u).dot(dx_) +
	      mu_eq * (Adx).dot(residual_in_y) +
	      mu_in * (tmp_a0_l.dot(tmp_b0_l) + tmp_a0_u.dot(tmp_b0_u));

	return a * alpha + b;
}


template <typename T, Layout LC>
auto oldNew_local_saddle_point_box(
		qp::Qpdata<T>& qpmodel,
		VectorView<T> ze,
		VectorView<T> dz_,
		T mu_in_inv,
		MatrixView<T, LC> C,
		VectorView<T> Cdx_,
		VectorView<T> residual_in_z_u_,
		VectorView<T> residual_in_z_l_,
		VectorView<T> d_dual_for_eq_,
		VectorView<T> dual_for_eq_,
		VectorView<T> d_primal_residual_eq_,
		VectorView<T> primal_residual_eq_,
		T& alpha,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& z_p,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dz_p,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_l,

		VectorViewMut<isize> _active_set_l,
		VectorViewMut<isize> _active_set_u,
		VectorViewMut<isize> _inactive_set,

		VectorViewMut<T> _tmp_d2_u,
		VectorViewMut<T> _tmp_d2_l,
		VectorViewMut<T> _tmp_d3,
		VectorViewMut<T> _tmp2_u,
		VectorViewMut<T> _tmp2_l,
		VectorViewMut<T> _tmp3_local_saddle_point

		) -> T {
	/*
	 * the function returns the unique minimum of the positive second order
	 * polynomial in alpha of the L2 norm of the following vector:
	 * concat((
	 *   H.dot(x) + g + rho_primal_proximal * (x-xe)
	 *   + A.transpose() × y
	 *   + C[active_inequalities_l,:].T × z[active_inequalities_l]
	 *   + C[active_inequalities_u,:].T × z[active_inequalities_u],
	 *   residual_eq - (y-ye)/mu_eq,
	 *
	 *   residual_in_u[active_inequalities_u]
	 *   - (z[active_inequalities_u]-ze[active_inequalities_u])/mu_in,
	 *
	 *   residual_in_l[active_inequalities_l]
	 *   - (z[active_inequalities_l]-ze[active_inequalities_l])/mu_in,
	 *
	 *   z[inactive_inequalities],
	 * ))
	 *
	 * with
	 * x = xe + alpha dx
	 * y = ye + alpha dy
	 * z[active_inequalities_u] = max((ze+alpha dz)[active_inequalities_u], 0)
	 * z[active_inequalities_l] = min((ze+alpha dz)[active_inequalities_l], 0)
	 *
	 * Furthermore
	 * residual_eq = A.dot(x) - b
	 * residual_in_u = C.dot(x) - u
	 * residual_in_l = C.dot(x) - l
	 * active_inequalities_u = residual_in_u + alpha Cdx >=0
	 * active_inequalities_l = residual_in_l + alpha Cdx <=0
	 * active_inequalities = active_inequalities_u + active_inequalities_l
	 * inactive_inequalities = ~active_inequalities
	 *
	 * To do so the L2 norm is expanded and the exact coefficients of the
	 * polynomial a0 alpha**2 + b0 alpha + c0 are derived.
	 * The argmin is then equal to -b0/2a0 if a0 != 0 and is changed INPLACE
	 * (erasing then alpha entry)
	 * the function returns the L2 norm of the merit function evaluated at the
	 * argmin value found
	 */

	auto C_copy = C.to_eigen();
	auto z_e = ze.to_eigen();
	auto dz = dz_.to_eigen();

	auto Cdx = Cdx_.to_eigen();
	auto residual_in_z_u = residual_in_z_u_.to_eigen();
	auto residual_in_z_l = residual_in_z_l_.to_eigen();
	auto d_dual_for_eq = d_dual_for_eq_.to_eigen().eval();
	auto dual_for_eq = dual_for_eq_.to_eigen().eval();
	auto d_primal_residual_eq = d_primal_residual_eq_.to_eigen();
	auto primal_residual_eq = primal_residual_eq_.to_eigen();

	tmp_u.noalias() = residual_in_z_u + alpha * Cdx;
	tmp_l.noalias() = residual_in_z_l + alpha * Cdx;

	isize num_active_u = 0;
	isize num_active_l = 0;
	isize num_inactive = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) >= T(0.)) {
			num_active_u += 1;
		}
		if (tmp_l(k) <= T(0.)) {
			num_active_l += 1;
		}
		if (tmp_u(k) < T(0.) && tmp_l(k) > T(0.)) {
			num_inactive += 1;
		}
	}

	auto active_set_u = _active_set_u.to_eigen();

	active_set_u.setZero();

	isize i_u = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) >= T(0.)) {
			active_set_u(i_u) = k;
			i_u += 1;
		}
	}
	auto active_set_l = _active_set_l.to_eigen();
	active_set_l.setZero();

	isize i_l = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_l(k) <= T(0.)) {
			active_set_l(i_l) = k;
			i_l += 1;
		}
	}
	auto inactive_set = _inactive_set.to_eigen();
	inactive_set.setZero();
	isize i_inact = 0;
	for (isize k = 0; k < qpmodel._n_in; ++k) {
		if (tmp_u(k) < T(0.) && tmp_l(k) > T(0.)) {
			inactive_set(i_inact) = k;
			i_inact += 1;
		}
	}
	
	// form the gradient
	z_p = z_e;
	dz_p = dz;

	for (isize k = 0; k < num_active_u; ++k) {
		T test = z_e(active_set_u(k)) + alpha * dz(active_set_u(k));
		if (test < 0) {
			z_p(active_set_u(k)) = 0;
			dz_p(active_set_u(k)) = 0;
		}
	}
	for (isize k = 0; k < num_active_l; ++k) {
		T test2 = z_e(active_set_l(k)) + alpha * dz(active_set_l(k));
		if (test2 > 0) {
			z_p(active_set_l(k)) = 0;
			dz_p(active_set_l(k)) = 0;
		}
	}
	// a0 computation
	
	auto tmp_d2_u = _tmp_d2_u.to_eigen();
	tmp_d2_u.setZero();
	auto tmp_d2_l = _tmp_d2_l.to_eigen();
	tmp_d2_l.setZero();
	auto tmp_d3 = _tmp_d3.to_eigen();
	tmp_d3.setZero();
	auto tmp2_u = _tmp2_u.to_eigen();
	tmp2_u.setZero();
	auto tmp2_l = _tmp2_l.to_eigen();
	tmp2_l.setZero();
	auto tmp3 = _tmp3_local_saddle_point.to_eigen();
	tmp3.setZero();

	for (isize k = 0; k < num_active_u; ++k) {
		d_dual_for_eq.noalias() += dz_p(active_set_u(k)) * C_copy.row(active_set_u(k));
		tmp_d2_u(k) = Cdx(active_set_u(k)) - dz_p(active_set_u(k)) * mu_in_inv;
	}
	for (isize k = 0; k < num_active_l; ++k) {
		d_dual_for_eq.noalias() += dz_p(active_set_l(k)) * C_copy.row(active_set_l(k));
		tmp_d2_l(k) = Cdx(active_set_l(k)) - dz_p(active_set_l(k)) * mu_in_inv;
	}

	for (isize k = 0; k < num_inactive; ++k) {
		tmp_d3(k) = dz_p(inactive_set(k));
	}
	T a0 = d_dual_for_eq.squaredNorm() + tmp_d2_u.squaredNorm() +
	       tmp_d2_l.squaredNorm() + tmp_d3.squaredNorm() +
	       d_primal_residual_eq.squaredNorm();
	// b0 computation
	for (isize k = 0; k < num_active_u; ++k) {
		dual_for_eq.noalias() += z_p(active_set_u(k)) * C_copy.row(active_set_u(k));
		tmp2_u(k) = residual_in_z_u(active_set_u(k)) - z_p(active_set_u(k)) * mu_in_inv;
	}
	for (isize k = 0; k < num_active_l; ++k) {
		dual_for_eq.noalias() += z_p(active_set_l(k)) * C_copy.row(active_set_l(k));
		tmp2_l(k) = residual_in_z_l(active_set_l(k)) - z_p(active_set_l(k)) * mu_in_inv;
	}
	for (isize k = 0; k < num_inactive; ++k) {
		tmp3(k) = z_p(inactive_set(k));
	}

	//T b0 = 2 * (d_dual_for_eq.dot(dual_for_eq) + tmp_d2_u.dot(tmp2_u) +
	//           tmp_d2_l.dot(tmp2_l) + tmp3.dot(tmp_d3) +
	//            primal_residual_eq.dot(d_primal_residual_eq));
	T b0 =  (d_dual_for_eq.dot(dual_for_eq) + tmp_d2_u.dot(tmp2_u) +
	           tmp_d2_l.dot(tmp2_l) + tmp3.dot(tmp_d3) +
	            primal_residual_eq.dot(d_primal_residual_eq));

	// c0 computation
	T c0 = dual_for_eq.squaredNorm() + tmp2_u.squaredNorm() + tmp3.squaredNorm() +
	       tmp2_l.squaredNorm() + primal_residual_eq.squaredNorm();
	// derivation of the loss function value and corresponding argmin alpha

	T res(0.);

	if (a0 != 0.) {
		//alpha = (-b0 / (2 * a0));
		alpha = -b0 / a0 ;
		//res = a0 * pow(alpha, T(2)) + b0 * alpha + c0;
		res = alpha * ( a0 * alpha + 2. * b0) + c0;
	} else if (b0 != 0) {
		alpha = (-c0 / (b0));
		res = b0 * alpha + c0;
	} else {
		alpha = 0.;
		res = c0;
	}

	return res;
}


template <typename T, Layout LC>
auto oldNew_initial_guess_LS(
		qp::Qpsettings<T>& qpsettings,
		qp::Qpdata<T>& qpmodel,
		VectorView<T> ze,
		VectorView<T> dz,
		VectorView<T> residual_in_z_l_,
		VectorView<T> residual_in_z_u_,
		VectorView<T> Cdx_,
		VectorView<T> d_dual_for_eq,
		VectorView<T> dual_for_eq,
		VectorView<T> d_primal_residual_eq,
		VectorView<T> primal_residual_eq,
		MatrixView<T,LC> C,
		T mu_in_inv,
		T rho,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& active_part_z,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& tmp_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& res,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& dz_p,

		VectorViewMut<isize> _active_set_l,
		VectorViewMut<isize> _active_set_u,
		VectorViewMut<isize> _inactive_set,

		VectorViewMut<T> _tmp_d2_u,
		VectorViewMut<T> _tmp_d2_l,
		VectorViewMut<T> _tmp_d3,
		VectorViewMut<T> _tmp2_u,
		VectorViewMut<T> _tmp2_l,
		VectorViewMut<T> _tmp3_local_saddle_point,

		std::vector<T>& alphas


		) -> T {
	/*
	 * Considering the following qp = (H, g, A, b, C, u,l) and a Newton step
	 * (dx,dy,dz) the fonction gives one optimal alpha minimizing the L2 norm
	 * of the following vector
	 * concat((
	 *   H.dot(x) + g + rho_primal_proximal * (x-xe)
	 *   + A.transpose() × y
	 *   + C[active_inequalities_l,:].T × z[active_inequalities_l]
	 *   + C[active_inequalities_u,:].T × z[active_inequalities_u],
	 *   residual_eq - (y-ye)/mu_eq,
	 *
	 *   residual_in_u[active_inequalities_u]
	 *   - (z[active_inequalities_u]-ze[active_inequalities_u])/mu_in,
	 *
	 *   residual_in_l[active_inequalities_l]
	 *   - (z[active_inequalities_l]-ze[active_inequalities_l])/mu_in,
	 *
	 *   z[inactive_inequalities],
	 * ))
	 *
	 * with
	 * x = xe + alpha dx
	 * y = ye + alpha dy
	 * z[active_inequalities_u] = max((ze+alpha dz)[active_inequalities_u], 0)
	 * z[active_inequalities_l] = min((ze+alpha dz)[active_inequalities_l], 0)
	 *
	 * Furthermore
	 * residual_eq = A.dot(x) - b
	 * residual_in_u = C.dot(x) - u
	 * residual_in_l = C.dot(x) - l
	 * active_inequalities_u = residual_in_u + alpha Cdx >=0
	 * active_inequalities_l = residual_in_l + alpha Cdx <=0
	 * active_inequalities = active_inequalities_u + active_inequalities_l
	 * inactive_inequalities = ~active_inequalities
	 *
	 * It can be shown that when one optimal active set is found for the qp
	 * problem, then the optimal alpha canceling (hence minimizing) the L2 norm
	 * of the merit function is unique and equal to 1
	 *
	 * If the optimal active set is not found, one optimal alpha found can not
	 * deviate new iterates formed from the sub problem solution
	 * To do so the algorithm has the following structure :
	 * 1/
	 * 1.1/ it computes the "nodes" alpha which cancel
	 * C.dot(xe+alpha dx) - u, C.dot(xe+alpha dx) - l and ze + alpha dz
	 *
	 * 2/
	 * 2.1/ it sorts the alpha nodes
	 *
	 * 2.2/ for each "node" it derives the L2 norm of the vector to minimize
	 * (see function: gradient_norm_computation_box) and stores it
	 *
	 * 3/ it defines all intervals on which the active set is constant
	 * 3.1/ it  define intervals (for ex with n+1 nodes):
	 * [alpha[0]-1;alpha[0]],[alpha[0],alpha[1]], ....; [alpha[n],alpha[n]+1]]
	 *
	 * 3.2/ for each interval
	 * it derives the mean node (alpha[i]+alpha[i+1])/2 and the corresponding
	 * active sets active_inequalities_u and active_inequalities_
	 * cap ze and d
	 *
	 * optimal lagrange multiplier z satisfy
	 * z[active_inequalities_u] = max((ze+alpha dz)[active_inequalities_u], 0)
	 * z[active_inequalities_l] = min((ze+alpha dz)[active_inequalities_l], 0
	 *
	 * 3.3/ on this interval the merit function is a second order polynomial in
	 * alpha
	 * the function "local_saddle_point_box" derives the exact minimum and
	 * corresponding merif function L2 norm (for this minimum
	 *
	 * 3.4/ if the argmin is within the interval [alpha[i],alpha[i+1]] is
	 * stores the argmin and corresponding L2 norm
	 *
	 * 4/ if the list of argmin obtained from intervals is not empty the
	 * algorithm return the one minimizing the most the merit function
	 * Otherwise, it returns the node minimizing the most the merit function
	 */

	T machine_eps = std::numeric_limits<T>::epsilon();
	T machine_inf = std::numeric_limits<T>::infinity();

	auto z_e = ze.to_eigen();
	auto dz_ = dz.to_eigen().eval();

	T alpha(1.);

	T alpha_n(1.);
	T gr_n(1.);
	T gr_interval(1.);
	T alpha_interval(1.);
	T alpha_(0.);

	/////////// STEP 1 ////////////
	// computing the "nodes" alphas which cancel  C.dot(xe+alpha dx) - u,
	// C.dot(xe+alpha dx) - l and ze + alpha dz  /////////////

	//std::vector<T> alphas;
	//alphas.reserve( 3*n_in );
	alphas.clear();

	// 1.1 add solutions of equation z+alpha dz = 0
	
	for (isize i = 0; i < qpmodel._n_in; i++) {
		if (std::abs(z_e(i)) != 0.) {
			alpha_ = -z_e(i) / (dz_(i) + machine_eps);
			if (std::abs(alpha_)< qpsettings._R){
				alphas.push_back(alpha_);
			}
		}
	}

	// 1.1 add solutions of equations C(x+alpha dx)-u +ze/mu_in = 0 and C(x+alpha
	// dx)-l +ze/mu_in = 0

	auto Cdx = Cdx_.to_eigen();
	auto residual_in_z_u = residual_in_z_u_.to_eigen();
	auto residual_in_z_l = residual_in_z_l_.to_eigen();

	for (isize i = 0; i < qpmodel._n_in; i++) {
		if (std::abs(Cdx(i)) != 0) {
			alpha_= -residual_in_z_u(i) / (Cdx(i) + machine_eps);
			if (std::abs(alpha_) < qpsettings._R){
				alphas.push_back(alpha_);
			}
			alpha_ = -residual_in_z_l(i) / (Cdx(i) + machine_eps);
			if (std::abs(alpha_) < qpsettings._R){
				alphas.push_back(alpha_);
			}
		}
	}

	isize n_alpha = alphas.size();
	
	if (n_alpha!=0) {
		//////// STEP 2 ////////
		// 2.1/ it sorts alpha nodes

		std::sort (alphas.begin(), alphas.begin()+n_alpha); 
		alphas.erase( std::unique( alphas.begin(), alphas.begin()+n_alpha), alphas.begin()+n_alpha );
		n_alpha = alphas.size();
		// 2.2/ for each node active set and associated gradient are computed

		bool first(true);

		for (isize i=0;i<n_alpha;++i) {
			alpha_ = alphas[i];
			if (std::abs(alpha_) < T(1.e6)) {
				
				// calcul de la norm du gradient du noeud
				T grad_norm = line_search::oldNew_gradient_norm_computation_box(
						qpmodel,
						ze,
						dz_,
						mu_in_inv,
						C,
						Cdx_,
						residual_in_z_u_,
						residual_in_z_l_,
						d_dual_for_eq,
						dual_for_eq,
						d_primal_residual_eq,
						primal_residual_eq,
						alpha_,

						active_part_z,
						tmp_u,
						tmp_l,
						aux_u,
						aux_l,
						res,

						_active_set_l,
						_active_set_u,
						_inactive_set

						);
				active_part_z.setZero();
				res.setZero();
				tmp_u.setZero();
				tmp_l.setZero();
				aux_u.setZero();
				aux_l.setZero();

				if (first){
					alpha_n = alpha_;
					gr_n = grad_norm;
					first = false;
				}else{
					if (grad_norm<gr_n){
						alpha_n = alpha_;
						gr_n = grad_norm;
					}
				} 
			} 
		}
		first = true;
		//////////STEP 3 ////////////
		// 3.1 : define intervals with alphas

		for (isize i = -1; i < n_alpha; ++i) {

			// 3.2 : it derives the mean node (alpha[i]+alpha[i+1])/2
			// the corresponding active sets active_inequalities_u and
			// active_inequalities_l cap ze and dz is derived through function
			// local_saddle_point_box

			if (i == -1){
				alpha_ = alphas[0] - T(0.5);
			} else if (i==n_alpha-1){
				alpha_ = alphas[n_alpha-1] +  T(0.5);
			} else{
				alpha_ = (alphas[i] + alphas[i + 1]) * T(0.5);
			}

			// 3.3 on this interval the merit function is a second order
			// polynomial in alpha
			// the function "local_saddle_point_box" derives the exact minimum
			// and corresponding merit function L2 norm (for this minimum)
			T associated_grad_2_norm = line_search::oldNew_local_saddle_point_box(
					qpmodel,
					ze,
					dz,
					mu_in_inv,
					C,
					Cdx_,
					residual_in_z_u_,
					residual_in_z_l_,
					d_dual_for_eq,
					dual_for_eq,
					d_primal_residual_eq,
					primal_residual_eq,
					alpha_,

					active_part_z,
					dz_p,
					tmp_u,
					tmp_l,
					
					_active_set_l,
					_active_set_u,
					_inactive_set,

					_tmp_d2_u,
					_tmp_d2_l,
					_tmp_d3,
					_tmp2_u,
					_tmp2_l,
					_tmp3_local_saddle_point
					
					);

			// 3.4 if the argmin is within the interval [alpha[i],alpha[i+1]] is
			// stores the argmin and corresponding L2 norm

			if (i == -1) {
				if (alpha_ <= alphas[0]) {
					
					if (first){
						first = false;
						alpha_interval = alpha_ ;
						gr_interval = associated_grad_2_norm;
					} else{
						if (associated_grad_2_norm<gr_interval){
							alpha_interval = alpha_ ;
							gr_interval = associated_grad_2_norm;
						}
					}
				}
			} else if (i == n_alpha-1) {
				if (alpha_ >= alphas[n_alpha - 1]) {
					
					if (first){
						first = false;
						alpha_interval = alpha_ ;
						gr_interval = associated_grad_2_norm;
					} else{
						if (associated_grad_2_norm<gr_interval){
							alpha_interval = alpha_ ;
							gr_interval = associated_grad_2_norm;
						}
					}
				}
			} else {
				if (alpha_ <= alphas[i + 1] && alphas[i] <= alpha_) {
					
					if (first){
						first = false;
						alpha_interval = alpha_ ;
						gr_interval = associated_grad_2_norm;
					} else{
						if (associated_grad_2_norm<gr_interval){
							alpha_interval = alpha_ ;
							gr_interval = associated_grad_2_norm;
						}
					}
				}
			}

		}

		///////// STEP 4 ///////////
		// if the list of argmin obtained from intervals is not empty the
		// algorithm return the one minimizing the most the merit function
		// Otherwise, it returns the node minimizing the most the merit
		// function

		if (gr_interval <= gr_n){
			alpha = alpha_interval;
		}else{
			alpha = alpha_n;
		}


	}

	return alpha;
}


template <typename T>
auto oldNew_correction_guess_LS(
		qp::Qpdata<T>& qpmodel,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Hdx,
		VectorView<T> dx,
		VectorView<T> g,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Adx,  
		Eigen::Matrix<T,Eigen::Dynamic,1>& Cdx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& residual_in_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& residual_in_z_u,
		Eigen::Matrix<T,Eigen::Dynamic,1>& residual_in_z_l,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> ye, 
		VectorView<T> ze,
		T mu_eq,
		T mu_in,

		T rho,
		
		VectorViewMut<T> _tmp_u,
		VectorViewMut<T> _tmp_l,
		VectorViewMut<isize> _active_set_u,
		VectorViewMut<isize> _active_set_l,

		VectorViewMut<T> _tmp_a0_u,
		VectorViewMut<T> _tmp_b0_u,
		VectorViewMut<T> _tmp_a0_l,
		VectorViewMut<T> _tmp_b0_l,

		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_u,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_l,

		std::vector<T>& alphas,

		Eigen::Matrix<T, Eigen::Dynamic, 1>& aux_u


		) -> T {

	/*
	 * The function follows the algorithm designed by qpalm
	 * (see algorithm 2 : https://arxiv.org/pdf/1911.02934.pdf)
	 *
	 * To do so it does the following step
	 * 1/
	 * 1.1/ Store solutions of equations
	 * C(x+alpha dx) - l + ze/mu_in = 0
	 * C(x+alpha dx) - u + ze/mu_in = 0
	 *
	 * 1.2/ Sort the alpha
	 * 2/
	 * 2.1
	 * For each positive alpha compute the first derivative of
	 * phi(alpha) = [proximal augmented lagrangian of the subproblem evaluated
	 *               at x_k + alpha dx]
	 * using function "gradient_norm_qpalm_box"
	 * By construction for alpha = 0,
	 *   phi'(alpha) <= 0
	 *   and phi'(alpha) goes to infinity with alpha
	 * hence it cancels uniquely at one optimal alpha*
	 *
	 * while phi'(alpha)<=0 store the derivative (noted last_grad_neg) and
	 * alpha (last_alpha_neg)
	 * the first time phi'(alpha) > 0 store the derivative (noted
	 * first_grad_pos) and alpha (first_alpha_pos), and break the loo
	 *
	 * 2.2
	 * If first_alpha_pos corresponds to the first positive alpha of previous
	 * loop, then do
	 *   last_alpha_neg = 0
	 *   last_grad_neg = phi'(0)
	 * using function "gradient_norm_qpalm_box"
	 *
	 * 2.3
	 * the optimal alpha is within the interval
	 * [last_alpha_neg,first_alpha_pos] and can be computed exactly as phi' is
	 * an affine function in alph
	 * alpha* = alpha_last_neg
	 *        - last_neg_grad * (alpha_first_pos - alpha_last_neg) /
	 *                          (first_pos_grad - last_neg_grad);
	 */

	T machine_eps = std::numeric_limits<T>::epsilon();

	auto x_ = x.to_eigen();
	auto z_e = ze.to_eigen();
	auto y_e = ye.to_eigen();

	T alpha(1.);
	T alpha_(1.);

	alphas.clear();
	//std::vector<T> alphas;
	//alphas.reserve( 2*n_in );

	///////// STEP 1 /////////
	// 1.1 add solutions of equations C(x+alpha dx)-l +ze/mu_in = 0 and C(x+alpha
	// dx)-u +ze/mu_in = 0

	for (isize i = 0; i < qpmodel._n_in; i++) {
		if (Cdx(i) != 0.) {
			alphas.push_back(-residual_in_z_u(i) / (Cdx(i) + machine_eps));
		}
		if (Cdx(i) != 0.) {
			alphas.push_back(-residual_in_z_l(i) / (Cdx(i) + machine_eps));
		}
	}

	isize n_alpha = alphas.size();

	if (n_alpha!=0) {
		// 1.2 sort the alphas

		std::sort (alphas.begin(), alphas.begin()+n_alpha); 
		alphas.erase( std::unique( alphas.begin(), alphas.begin()+n_alpha), alphas.begin()+n_alpha );
		n_alpha = alphas.size();

		////////// STEP 2 ///////////

		T last_neg_grad = 0;
		T alpha_last_neg = 0;
		T first_pos_grad = 0;
		T alpha_first_pos = 0;

		//for (auto a : alphas) {
		for (isize i = 0;i<n_alpha;++i){
			alpha_ = alphas[i];
			if (alpha_ > machine_eps) {

					/*
					 * 2.1
					 * For each positive alpha compute the first derivative of
					 * phi(alpha) = [proximal augmented lagrangian of the
					 *               subproblem evaluated at x_k + alpha dx]
					 * using function "gradient_norm_qpalm_box"
					 *
					 * (By construction for alpha = 0,  phi'(alpha) <= 0 and
					 * phi'(alpha) goes to infinity with alpha hence it cancels
					 * uniquely at one optimal alpha*
					 *
					 * while phi'(alpha)<=0 store the derivative (noted
					 * last_grad_neg) and alpha (last_alpha_neg
					 * the first time phi'(alpha) > 0 store the derivative
					 * (noted first_grad_pos) and alpha (first_alpha_pos), and
					 * break the loop
					 */
					T gr = line_search::oldNew_gradient_norm_qpalm_box(
							qpmodel,
							x,
							xe,
							dx,
							mu_eq,
							mu_in,
							rho,
							alpha_,
							VectorView<T>{from_eigen,Hdx},
							g,
							VectorView<T>{from_eigen,Adx},
							VectorView<T>{from_eigen, residual_in_y},
							VectorView<T>{from_eigen, residual_in_z_u},
							VectorView<T>{from_eigen, residual_in_z_l},
							VectorView<T>{from_eigen, Cdx},
							
							_tmp_u,
							_tmp_l,
							_active_set_u,
							_active_set_l,

							_tmp_a0_u,
							_tmp_b0_u,
							_tmp_a0_l,
							_tmp_b0_l,

							l_active_set_n_u,
							l_active_set_n_l,

							aux_u

							);

					if (gr < T(0)) {
						alpha_last_neg = alpha_;
						last_neg_grad = gr;
					} else {
						first_pos_grad = gr;
						alpha_first_pos = alpha_;
						break;
					}
			}
		}

		/*
		 * 2.2
		 * If first_alpha_pos corresponds to the first positive alpha of
		 * previous loop, then do
		 * last_alpha_neg = 0 and last_grad_neg = phi'(0) using function
		 * "gradient_norm_qpalm_box"
		 */
		if (last_neg_grad == T(0)) {
			alpha_last_neg = T(0);
			T gr = line_search::oldNew_gradient_norm_qpalm_box(
					qpmodel,
					x,
					xe,
					dx,
					mu_eq,
					mu_in,
					rho,
					alpha_last_neg,
					VectorView<T>{from_eigen,Hdx},
					g,
					VectorView<T>{from_eigen,Adx},
					VectorView<T>{from_eigen, residual_in_y},
					VectorView<T>{from_eigen, residual_in_z_u},
					VectorView<T>{from_eigen, residual_in_z_l},
					VectorView<T>{from_eigen, Cdx},

					_tmp_u,
					_tmp_l,
					_active_set_u,
					_active_set_l,

					_tmp_a0_u,
					_tmp_b0_u,
					_tmp_a0_l,
					_tmp_b0_l, 

					l_active_set_n_u,
					l_active_set_n_l,

					aux_u

					);
			last_neg_grad = gr;
		}

		/*
		 * 2.3
		 * the optimal alpha is within the interval
		 * [last_alpha_neg,first_alpha_pos] and can be computed exactly as phi'
		 * is an affine function in alpha
		 */
		alpha = alpha_last_neg - last_neg_grad *
		                             (alpha_first_pos - alpha_last_neg) /
		                             (first_pos_grad - last_neg_grad);
	}	
	return alpha;
}

template <typename T>
void oldNew_active_set_change(
		qp::Qpdata<T>& qpmodel,
		qp::QpViewBox<T> qp_scaled,
		VectorView<bool> new_active_set_,
		VectorViewMut<isize> current_bijection_map_,
		Eigen::Matrix<isize, Eigen::Dynamic, 1>& new_bijection_map,
		ldlt::Ldlt<T>& ldl,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dw_aug,
		T rho,
		T mu_in_inv,
		isize& n_c) {

	/*
	 * arguments
	 * 1/ new_active_set : a vector which contains new active set of the
	 * problem, namely if
	 * new_active_set_u = Cx_k-u +z_k/mu_in>= 0
	 * new_active_set_l = Cx_k-l +z_k/mu_in<=
	 * then new_active_set = new_active_set_u OR new_active_set_
	 *
	 * 2/ current_bijection_map : a vector for which each entry corresponds to
	 * the current row of C of the current factorization
	 *
	 * for example, naming C_initial the initial C matrix of the problem, and
	 * C_current the one of the current factorization, the
	 * C_initial[i,:] = C_current[current_bijection_mal[i],:] for all
	 *
	 * 3/ n_c : the current number of active_inequalities
	 * This algorithm ensures that for all new version of C_current in the LDLT
	 * factorization all row index i < n_c correspond to current active indexes
	 * (all other correspond to inactive rows
	 *
	 * To do so,
	 * 1/ for initialization
	 * 1.1/ new_bijection_map = current_bijection_map
	 * 1.2/ n_c_f = n_
	 *
	 * 2/ All active indexes of the current bijection map (i.e
	 * current_bijection_map(i) < n_c by assumption) which are not active
	 * anymore in the new active set (new_active_set(i)=false are put at the
	 * end of new_bijection_map, i.
	 *
	 * 2.1/ for all j if new_bijection_map(j) > new_bijection_map(i), then
	 * new_bijection_map(j)-=1
	 * 2.2/ n_c_f -=1
	 * 2.3/ new_bijection_map(i) = n_in-1
	 *
	 * 3/ All active indexe of the new active set (new_active_set(i) == true)
	 * which are not active in the new_bijection_map (new_bijection_map(i) >=
	 * n_c_f) are put at the end of the current version of C, i.e
	 * 3.1/ if new_bijection_map(j) < new_bijection_map(i) &&
	 * new_bijection_map(j) >= n_c_f then new_bijection_map(j)+=1
	 * 3.2/ new_bijection_map(i) = n_c_f
	 * 3.3/ n_c_f +=1
	 *
	 * It returns finally the new_bijection_map, for which
	 * new_bijection_map(n_in) = n_c_f
	 */
			
	auto current_bijection_map = current_bijection_map_.to_eigen();
	auto new_active_set = new_active_set_.to_eigen();
	auto C_ = qp_scaled.C.to_eigen();
	dw_aug.setZero();
	
	isize n_c_f = n_c;
	new_bijection_map = current_bijection_map;

	// suppression pour le nouvel active set, ajout dans le nouvel unactive set

	T mu_in_inv_neg = -mu_in_inv;
	for (isize i = 0; i < qpmodel._n_in; i++) {
		if (current_bijection_map(i) < n_c) {
			if (!new_active_set(i)) {
				// delete current_bijection_map(i)
				ldl.delete_at(new_bijection_map(i) + qpmodel._dim + qpmodel._n_eq);

				for (isize j = 0; j < qpmodel._n_in; j++) {
					if (new_bijection_map(j) > new_bijection_map(i)) {
						new_bijection_map(j) -= 1;    
					}
				}
				n_c_f -= 1;
				new_bijection_map(i) = qpmodel._n_in - 1;

			}
		} 
	}

	// ajout au nouvel active set, suppression pour le nouvel unactive set

	for (isize i = 0; i < qpmodel._n_in; i++) {
		if (new_active_set(i)) {
			if (new_bijection_map(i) >= n_c_f) {
				// add at the end
				
				dw_aug.setZero();
				dw_aug.head(qpmodel._dim) = C_.row(i);
				dw_aug(qpmodel._dim + qpmodel._n_eq + n_c_f) = mu_in_inv_neg; // mu stores the inverse of mu
				ldl.insert_at(qpmodel._n_eq + qpmodel._dim + n_c_f, dw_aug.head(n_c_f+1+qpmodel._n_eq+qpmodel._dim));

				for (isize j = 0; j < qpmodel._n_in; j++) {
					if (new_bijection_map(j) < new_bijection_map(i) &&
						new_bijection_map(j) >= n_c_f) {
						new_bijection_map(j) += 1;
					}
				}
				new_bijection_map(i) = n_c_f;
				n_c_f += 1;

			}
		}
	}
	n_c = n_c_f;
	current_bijection_map = new_bijection_map;
	dw_aug.setZero();
}

} // namespace line_search
} // namespace qp

#endif /* end of include guard INRIA_LDLT_OLD_NEW_LINE_SEARCH_HPP_2TUXO5DFS */