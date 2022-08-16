//
// Copyright (c) 2022 INRIA
//
/**
 * @file wrapper.hpp
 */

#ifndef PROXSUITE_QP_SPARSE_WRAPPER_HPP
#define PROXSUITE_QP_SPARSE_WRAPPER_HPP
#include <proxsuite/proxqp/results.hpp>
#include <proxsuite/proxqp/settings.hpp>
#include <proxsuite/proxqp/sparse/solver.hpp>
#include <proxsuite/proxqp/sparse/helpers.hpp>

namespace proxsuite {
namespace proxqp {
namespace sparse {
///
/// @brief This class defines the API of PROXQP solver with sparse backend.
///
/*!
 * Wrapper class for using proxsuite API with dense backend
 * for solving linearly constrained convex QP problem using ProxQp algorithm.
 *
 * Example usage:
 * ```cpp
#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <proxsuite/proxqp/dense/dense.hpp>
#include <proxsuite/linalg/veg/util/dbg.hpp>
#include <util.hpp>

using T = double;
using I = c_int;
auto main() -> int {

        // Generate a random QP problem with primal variable dimension of size
dim; n_eq equality constraints and n_in inequality constraints
        ::proxsuite::proxqp::test::rand::set_seed(1);
        proxqp::isize dim = 10;
        proxqp::isize n_eq(dim / 4);
        proxqp::isize n_in(dim / 4);
        T strong_convexity_factor(1.e-2);
        T sparsity_factor = 0.15; // controls the sparsity of each matrix of the
problem generated T eps_abs = T(1e-9); double p = 1.0; T conditioning(10.0);
        auto H =
::proxsuite::proxqp::test::rand::sparse_positive_definite_rand(n, conditioning,
p); auto g = ::proxsuite::proxqp::test::rand::vector_rand<T>(n); auto A =
::proxsuite::proxqp::test::rand::sparse_matrix_rand<T>(n_eq,n, p); auto b =
::proxsuite::proxqp::test::rand::vector_rand<T>(n_eq); auto C =
::proxsuite::proxqp::test::rand::sparse_matrix_rand<T>(n_in,n, p); auto l =
::proxsuite::proxqp::test::rand::vector_rand<T>(n_in); auto u = (l.array() +
1).matrix().eval();

        proxqp::sparse::QP<T,I> Qp(n, n_eq, n_in);
        Qp.settings.eps_abs = 1.E-9;
        Qp.settings.verbose = true;
        Qp.setup_sparse_matrices(H,g,A,b,C,u,l);
        Qp.solve();

        // Solve the problem
        proxqp::sparse::QP<T,I> Qp(n, n_eq, n_in);
        Qp.settings.eps_abs = 1.E-9;
        Qp.settings.verbose = true;
        Qp.setup_sparse_matrices(H,g,A,b,C,u,l);
        Qp.solve();

        // Verify solution accuracy
        T pri_res = std::max(
                        (qp.A * Qp.results.x - qp.b).lpNorm<Eigen::Infinity>(),
                        (proxqp::dense::positive_part(qp.C * Qp.results.x -
qp.u) + proxqp::dense::negative_part(qp.C * Qp.results.x - qp.l))
                                        .lpNorm<Eigen::Infinity>());
        T dua_res = (qp.H * Qp.results.x + qp.g + qp.A.transpose() *
Qp.results.y + qp.C.transpose() * Qp.results.z) .lpNorm<Eigen::Infinity>();
        VEG_ASSERT(pri_res <= eps_abs);
        VEG_ASSERT(dua_res <= eps_abs);

        // Some solver statistics
        std::cout << "------solving qp with dim: " << dim
                                                << " neq: " << n_eq << " nin: "
<< n_in << std::endl; std::cout << "primal residual: " << pri_res << std::endl;
        std::cout << "dual residual: " << dua_res << std::endl;
        std::cout << "total number of iteration: " << Qp.results.info.iter
                                                << std::endl;
}
 * ```
 */
template<typename T, typename I>
struct QP
{
  Results<T> results;
  Settings<T> settings;
  Model<T, I> model;
  Workspace<T, I> work;
  preconditioner::RuizEquilibration<T, I> ruiz;
  /*!
   * Default constructor using the dimension of the matrices in entry.
   * @param _dim primal variable dimension.
   * @param _n_eq number of equality constraints.
   * @param _n_in number of inequality constraints.
   */
  QP(isize _dim, isize _n_eq, isize _n_in)
    : results(_dim, _n_eq, _n_in)
    , settings()
    , model(_dim, _n_eq, _n_in)
    , work()
    , ruiz(_dim, _n_eq + _n_in, 1e-3, 10, preconditioner::Symmetry::UPPER)
  {

    work.timer.stop();
    work.internal.do_symbolic_fact = true;
  }
  /*!
   * Default constructor using the sparsity structure of the matrices in entry.
   * @param H boolean mask of the quadratic cost input defining the QP model.
   * @param A boolean mask of the equality constraint matrix input defining the
   * QP model.
   * @param C boolean mask of the inequality constraint matrix input defining
   * the QP model.
   */
  QP(const SparseMat<bool, I>& H,
     const SparseMat<bool, I>& A,
     const SparseMat<bool, I>& C)
    : QP(H.rows(), A.rows(), C.rows())
  {
    if (settings.compute_timings) {
      work.timer.stop();
      work.timer.start();
    }
    SparseMat<bool, I> H_triu = H.template triangularView<Eigen::Upper>();
    SparseMat<bool, I> AT = A.transpose();
    SparseMat<bool, I> CT = C.transpose();
    proxsuite::linalg::sparse::MatRef<bool, I> Href = {
      proxsuite::linalg::sparse::from_eigen, H_triu
    };
    proxsuite::linalg::sparse::MatRef<bool, I> ATref = {
      proxsuite::linalg::sparse::from_eigen, AT
    };
    proxsuite::linalg::sparse::MatRef<bool, I> CTref = {
      proxsuite::linalg::sparse::from_eigen, CT
    };
    work.setup_symbolic_factorizaton(
      model, Href.symbolic(), ATref.symbolic(), CTref.symbolic());
    if (settings.compute_timings) {
      results.info.setup_time = work.timer.elapsed().user; // in microseconds
    }
  }

  /*!
   * Setups the QP model (with sparse matrix format) and equilibrates it.
   * @param H quadratic cost input defining the QP model.
   * @param g linear cost input defining the QP model.
   * @param A equality constraint matrix input defining the QP model.
   * @param b equality constraint vector input defining the QP model.
   * @param C inequality constraint matrix input defining the QP model.
   * @param u lower inequality constraint vector input defining the QP model.
   * @param l lower inequality constraint vector input defining the QP model.
   * @param compute_preconditioner boolean parameter for executing or not the
   * preconditioner.
   * @param rho proximal step size wrt primal variable.
   * @param mu_eq proximal step size wrt equality constrained multiplier.
   * @param mu_in proximal step size wrt inequality constrained multiplier.
   */
  void init(std::optional<SparseMat<T, I>> H,
            std::optional<VecRef<T>> g,
            std::optional<SparseMat<T, I>> A,
            std::optional<VecRef<T>> b,
            std::optional<SparseMat<T, I>> C,
            std::optional<VecRef<T>> u,
            std::optional<VecRef<T>> l,
            bool compute_preconditioner_ = true,
            std::optional<T> rho = std::nullopt,
            std::optional<T> mu_eq = std::nullopt,
            std::optional<T> mu_in = std::nullopt)
  {
    if (settings.compute_timings) {
      work.timer.stop();
      work.timer.start();
    }
    if (g != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        g.value().rows(),
        model.dim,
        "the dimension wrt the primal variable x variable for initializing g "
        "is not valid.");
    }
    if (b != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        b.value().rows(),
        model.n_eq,
        "the dimension wrt equality constrained variables for initializing b "
        "is not valid.");
    }
    if (u != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        u.value().rows(),
        model.n_in,
        "the dimension wrt inequality constrained variables for initializing u "
        "is not valid.");
    }
    if (l != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        l.value().rows(),
        model.n_in,
        "the dimension wrt inequality constrained variables for initializing l "
        "is not valid.");
    }
    if (H != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        H.value().rows(),
        model.dim,
        "the row dimension for initializing H is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        H.value().cols(),
        model.dim,
        "the column dimension for initializing H is not valid.");
    }
    if (A != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        A.value().rows(),
        model.n_eq,
        "the row dimension for initializing A is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        A.value().cols(),
        model.dim,
        "the column dimension for initializing A is not valid.");
    }
    if (C != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        C.value().rows(),
        model.n_in,
        "the row dimension for initializing C is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        C.value().cols(),
        model.dim,
        "the column dimension for initializing C is not valid.");
    }
    work.internal.proximal_parameter_update = false;
    PreconditionerStatus preconditioner_status;
    if (compute_preconditioner_) {
      preconditioner_status = proxsuite::proxqp::PreconditionerStatus::EXECUTE;
    } else {
      preconditioner_status = proxsuite::proxqp::PreconditionerStatus::IDENTITY;
    }
    proxsuite::proxqp::sparse::update_proximal_parameters(
      results, work, rho, mu_eq, mu_in);

    if (g != std::nullopt) {
      model.g = g.value();
    } // else qpmodel.g remains initialzed to a matrix with zero elements or
      // zero shape
    if (b != std::nullopt) {
      model.b = b.value();
    } // else qpmodel.b remains initialzed to a matrix with zero elements or
      // zero shape
    if (u != std::nullopt) {
      model.u = u.value();
    } // else qpmodel.u remains initialzed to a matrix with zero elements or
      // zero shape
    if (l != std::nullopt) {
      model.l = l.value();
    } // else qpmodel.l remains initialzed to a matrix with zero elements or
      // zero shape

    // avoid allocations when H is not nullopt
    SparseMat<T, I> AT(model.dim, model.n_eq);
    if (A != std::nullopt) {
      AT = (A.value()).transpose();
    } else {
      AT.setZero();
    }
    SparseMat<T, I> CT(model.dim, model.n_in);
    if (C != std::nullopt) {
      CT = (C.value()).transpose();
    } else {
      CT.setZero();
    }
    if (H != std::nullopt) {
      SparseMat<T, I> H_triu =
        (H.value()).template triangularView<Eigen::Upper>();
      sparse::QpView<T, I> qp = {
        { proxsuite::linalg::sparse::from_eigen, H_triu },
        { proxsuite::linalg::sparse::from_eigen, model.g },
        { proxsuite::linalg::sparse::from_eigen, AT },
        { proxsuite::linalg::sparse::from_eigen, model.b },
        { proxsuite::linalg::sparse::from_eigen, CT },
        { proxsuite::linalg::sparse::from_eigen, model.l },
        { proxsuite::linalg::sparse::from_eigen, model.u }
      };
      qp_setup(qp, results, model, work, settings, ruiz, preconditioner_status);
    } else {
      SparseMat<T, I> H_triu(model.dim, model.dim);
      H_triu.setZero();
      H_triu = (H.value()).template triangularView<Eigen::Upper>();
      sparse::QpView<T, I> qp = {
        { proxsuite::linalg::sparse::from_eigen, H_triu },
        { proxsuite::linalg::sparse::from_eigen, model.g },
        { proxsuite::linalg::sparse::from_eigen, AT },
        { proxsuite::linalg::sparse::from_eigen, model.b },
        { proxsuite::linalg::sparse::from_eigen, CT },
        { proxsuite::linalg::sparse::from_eigen, model.l },
        { proxsuite::linalg::sparse::from_eigen, model.u }
      };
      qp_setup(qp, results, model, work, settings, ruiz, preconditioner_status);
    }

    if (settings.compute_timings) {
      results.info.setup_time += work.timer.elapsed().user; // in microseconds
    }
  };
  /*!
   * Updates the QP model (with sparse matrix format) and re-equilibrates it if
   * specified by the user. If matrices in entry are not null, the update is
   * effective only if the sparsity structure of entry is the same as the one
   * used for the initialization.
   * @param H_ quadratic cost input defining the QP model.
   * @param g_ linear cost input defining the QP model.
   * @param A_ equality constraint matrix input defining the QP model.
   * @param b_ equality constraint vector input defining the QP model.
   * @param C_ inequality constraint matrix input defining the QP model.
   * @param u_ lower inequality constraint vector input defining the QP model.
   * @param l_ lower inequality constraint vector input defining the QP model.
   * @param update_preconditioner_ bool parameter for updating or not the
   * preconditioner and the associated scaled model.
   * @param rho proximal step size wrt primal variable.
   * @param mu_eq proximal step size wrt equality constrained multiplier.
   * @param mu_in proximal step size wrt inequality constrained multiplier.
   */
  void update(const std::optional<SparseMat<T, I>> H_,
              std::optional<VecRef<T>> g_,
              const std::optional<SparseMat<T, I>> A_,
              std::optional<VecRef<T>> b_,
              const std::optional<SparseMat<T, I>> C_,
              std::optional<VecRef<T>> u_,
              std::optional<VecRef<T>> l_,
              bool update_preconditioner_ = true,
              std::optional<T> rho = std::nullopt,
              std::optional<T> mu_eq = std::nullopt,
              std::optional<T> mu_in = std::nullopt)
  {
    if (settings.compute_timings) {
      work.timer.stop();
      work.timer.start();
    }
    work.internal.dirty = false;
    work.internal.proximal_parameter_update = false;
    PreconditionerStatus preconditioner_status;
    if (update_preconditioner_) {
      preconditioner_status = proxsuite::proxqp::PreconditionerStatus::EXECUTE;
    } else {
      preconditioner_status = proxsuite::proxqp::PreconditionerStatus::KEEP;
    }
    isize n = model.dim;
    isize n_eq = model.n_eq;
    isize n_in = model.n_in;
    proxsuite::linalg::sparse::MatMut<T, I> kkt_unscaled =
      model.kkt_mut_unscaled();

    auto kkt_top_n_rows = detail::top_rows_mut_unchecked(
      proxsuite::linalg::veg::unsafe, kkt_unscaled, n);

    proxsuite::linalg::sparse::MatMut<T, I> H_unscaled =
      detail::middle_cols_mut(kkt_top_n_rows, 0, n, model.H_nnz);

    proxsuite::linalg::sparse::MatMut<T, I> AT_unscaled =
      detail::middle_cols_mut(kkt_top_n_rows, n, n_eq, model.A_nnz);

    proxsuite::linalg::sparse::MatMut<T, I> CT_unscaled =
      detail::middle_cols_mut(kkt_top_n_rows, n + n_eq, n_in, model.C_nnz);

    // check the model is valid
    if (g_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(g_.value().rows(),
                                    model.dim,
                                    "the dimension wrt the primal variable x "
                                    "variable for updating g is not valid.");
    }
    if (b_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(b_.value().rows(),
                                    model.n_eq,
                                    "the dimension wrt equality constrained "
                                    "variables for updating b is not valid.");
    }
    if (u_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(u_.value().rows(),
                                    model.n_in,
                                    "the dimension wrt inequality constrained "
                                    "variables for updating u is not valid.");
    }
    if (l_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(l_.value().rows(),
                                    model.n_in,
                                    "the dimension wrt inequality constrained "
                                    "variables for updating l is not valid.");
    }
    if (H_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        H_.value().rows(),
        model.dim,
        "the row dimension for updating H is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        H_.value().cols(),
        model.dim,
        "the column dimension for updating H is not valid.");
    }
    if (A_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        A_.value().rows(),
        model.n_eq,
        "the row dimension for updating A is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        A_.value().cols(),
        model.dim,
        "the column dimension for updating A is not valid.");
    }
    if (C_ != std::nullopt) {
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        C_.value().rows(),
        model.n_in,
        "the row dimension for updating C is not valid.");
      PROXSUITE_CHECK_ARGUMENT_SIZE(
        C_.value().cols(),
        model.dim,
        "the column dimension for updating C is not valid.");
    }

    // update the model

    if (g_ != std::nullopt) {
      model.g = g_.value();
    }
    if (b_ != std::nullopt) {
      model.b = b_.value();
    }
    if (u_ != std::nullopt) {
      model.u = u_.value();
    }
    if (l_ != std::nullopt) {
      model.l = l_.value();
    }
    if (H_ != std::nullopt) {
      SparseMat<T, I> H_triu =
        H_.value().template triangularView<Eigen::Upper>();
      if (A_ != std::nullopt) {
        if (C_ != std::nullopt) {
          bool res =
            have_same_structure(
              H_unscaled.as_const(),
              { proxsuite::linalg::sparse::from_eigen, H_triu }) &&
            have_same_structure(AT_unscaled.as_const(),
                                { proxsuite::linalg::sparse::from_eigen,
                                  SparseMat<T, I>(A_.value().transpose()) }) &&
            have_same_structure(CT_unscaled.as_const(),
                                { proxsuite::linalg::sparse::from_eigen,
                                  SparseMat<T, I>(C_.value().transpose()) });
          /* TO PUT IN DEBUG MODE
          std::cout << "have same structure = " << res << std::endl;
          */
          if (res) {
            copy(H_unscaled,
                 { proxsuite::linalg::sparse::from_eigen,
                   H_triu }); // copy rhs into lhs
            copy(
              AT_unscaled,
              { proxsuite::linalg::sparse::from_eigen,
                SparseMat<T, I>(A_.value().transpose()) }); // copy rhs into lhs
            copy(
              CT_unscaled,
              { proxsuite::linalg::sparse::from_eigen,
                SparseMat<T, I>(C_.value().transpose()) }); // copy rhs into lhs
          }
        } else {
          bool res =
            have_same_structure(
              H_unscaled.as_const(),
              { proxsuite::linalg::sparse::from_eigen, H_triu }) &&
            have_same_structure(AT_unscaled.as_const(),
                                { proxsuite::linalg::sparse::from_eigen,
                                  SparseMat<T, I>(A_.value().transpose()) });
          /* TO PUT IN DEBUG MODE
          std::cout << "have same structure = " << res << std::endl;
          */
          if (res) {
            copy(H_unscaled,
                 { proxsuite::linalg::sparse::from_eigen,
                   H_triu }); // copy rhs into lhs
            copy(
              AT_unscaled,
              { proxsuite::linalg::sparse::from_eigen,
                SparseMat<T, I>(A_.value().transpose()) }); // copy rhs into lhs
          }
        }
      } else if (C_ != std::nullopt) {
        bool res =
          have_same_structure(
            H_unscaled.as_const(),
            { proxsuite::linalg::sparse::from_eigen, H_triu }) &&
          have_same_structure(CT_unscaled.as_const(),
                              { proxsuite::linalg::sparse::from_eigen,
                                SparseMat<T, I>(C_.value().transpose()) });
        /* TO PUT IN DEBUG MODE
        std::cout << "have same structure = " << res << std::endl;
        */
        if (res) {
          copy(H_unscaled,
               { proxsuite::linalg::sparse::from_eigen,
                 H_triu }); // copy rhs into lhs
          copy(
            CT_unscaled,
            { proxsuite::linalg::sparse::from_eigen,
              SparseMat<T, I>(C_.value().transpose()) }); // copy rhs into lhs
        }
      } else {

        bool res = have_same_structure(
          H_unscaled.as_const(),
          { proxsuite::linalg::sparse::from_eigen, H_triu });
        /* TO PUT IN DEBUG MODE
        std::cout << "have same structure = " << res << std::endl;
        */
        if (res) {
          copy(H_unscaled,
               { proxsuite::linalg::sparse::from_eigen,
                 H_.value() }); // copy rhs into lhs
        }
      }
    } else if (A_ != std::nullopt) {
      if (C_ != std::nullopt) {
        bool res =
          have_same_structure(AT_unscaled.as_const(),
                              { proxsuite::linalg::sparse::from_eigen,
                                SparseMat<T, I>(A_.value().transpose()) }) &&
          have_same_structure(CT_unscaled.as_const(),
                              { proxsuite::linalg::sparse::from_eigen,
                                SparseMat<T, I>(C_.value().transpose()) });
        /* TO PUT IN DEBUG MODE
        std::cout << "have same structure = " << res << std::endl;
        */
        if (res) {
          copy(
            AT_unscaled,
            { proxsuite::linalg::sparse::from_eigen,
              SparseMat<T, I>(A_.value().transpose()) }); // copy rhs into lhs
          copy(
            CT_unscaled,
            { proxsuite::linalg::sparse::from_eigen,
              SparseMat<T, I>(C_.value().transpose()) }); // copy rhs into lhs
        }
      } else {
        bool res =
          have_same_structure(AT_unscaled.as_const(),
                              { proxsuite::linalg::sparse::from_eigen,
                                SparseMat<T, I>(A_.value().transpose()) });
        /* TO PUT IN DEBUG MODE
        std::cout << "have same structure = " << res << std::endl;
        */
        if (res) {
          copy(
            AT_unscaled,
            { proxsuite::linalg::sparse::from_eigen,
              SparseMat<T, I>(A_.value().transpose()) }); // copy rhs into lhs
        }
      }
    } else if (C_ != std::nullopt) {
      bool res =
        have_same_structure(CT_unscaled.as_const(),
                            { proxsuite::linalg::sparse::from_eigen,
                              SparseMat<T, I>(C_.value().transpose()) });
      /* TO PUT IN DEBUG MODE
      std::cout << "have same structure = " << res << std::endl;
      */
      if (res) {
        copy(CT_unscaled,
             { proxsuite::linalg::sparse::from_eigen,
               SparseMat<T, I>(C_.value().transpose()) }); // copy rhs into lhs
      }
    }

    SparseMat<T, I> H_triu =
      H_unscaled.to_eigen().template triangularView<Eigen::Upper>();
    sparse::QpView<T, I> qp = {
      { proxsuite::linalg::sparse::from_eigen, H_triu },
      { proxsuite::linalg::sparse::from_eigen, model.g },
      { proxsuite::linalg::sparse::from_eigen, AT_unscaled.to_eigen() },
      { proxsuite::linalg::sparse::from_eigen, model.b },
      { proxsuite::linalg::sparse::from_eigen, CT_unscaled.to_eigen() },
      { proxsuite::linalg::sparse::from_eigen, model.l },
      { proxsuite::linalg::sparse::from_eigen, model.u }
    };
    proxsuite::proxqp::sparse::update_proximal_parameters(
      results, work, rho, mu_eq, mu_in);
    qp_setup(qp,
             results,
             model,
             work,
             settings,
             ruiz,
             preconditioner_status); // store model value + performs scaling
                                     // according to chosen options
    if (settings.compute_timings) {
      results.info.setup_time = work.timer.elapsed().user; // in microseconds
    }
  };

  /*!
   * Solves the QP problem using PRXOQP algorithm.
   */
  void solve()
  {
    qp_solve( //
      results,
      model,
      settings,
      work,
      ruiz);
  };
  /*!
   * Solves the QP problem using PROXQP algorithm and a warm start.
   * @param x primal warm start.
   * @param y dual equality warm start.
   * @param z dual inequality warm start.
   */
  void solve(std::optional<VecRef<T>> x,
             std::optional<VecRef<T>> y,
             std::optional<VecRef<T>> z)
  {
    proxsuite::proxqp::sparse::warm_start(x, y, z, results, settings, model);
    qp_solve( //
      results,
      model,
      settings,
      work,
      ruiz);
  };
  /*!
   * Clean-ups solver's results.
   */
  void cleanup() { results.cleanup(); }
};
/*!
 * Solves the QP problem using PROXQP algorithm without the need to define a QP
 * object, with matrices defined by Dense Eigen matrices. It is possible to set
 * up some of the solver parameters (warm start, initial guess option, proximal
 * step sizes, absolute and relative accuracies, maximum number of iterations,
 * preconditioner execution).
 * @param H quadratic cost input defining the QP model.
 * @param g linear cost input defining the QP model.
 * @param A equality constraint matrix input defining the QP model.
 * @param b equality constraint vector input defining the QP model.
 * @param C inequality constraint matrix input defining the QP model.
 * @param u lower inequality constraint vector input defining the QP model.
 * @param l lower inequality constraint vector input defining the QP model.
 * @param x primal warm start.
 * @param y dual equality constraint warm start.
 * @param z dual inequality constraint warm start.
 * @param verbose if set to true, the solver prints more information about each
 * iteration.
 * @param compute_preconditioner boolean parameter for executing or not the
 * preconditioner.
 * @param compute_timings boolean parameter for computing the solver timings.
 * @param rho proximal step size wrt primal variable.
 * @param mu_eq proximal step size wrt equality constrained multiplier.
 * @param mu_in proximal step size wrt inequality constrained multiplier.
 * @param eps_abs absolute accuracy threshold.
 * @param eps_rel relative accuracy threshold.
 * @param max_iter maximum number of iteration.
 * @param initial_guess initial guess option for warm starting or not the
 * initial iterate values.
 */
template<typename T, typename I>
proxqp::Results<T>
solve(
  std::optional<SparseMat<T, I>> H,
  std::optional<VecRef<T>> g,
  std::optional<SparseMat<T, I>> A,
  std::optional<VecRef<T>> b,
  std::optional<SparseMat<T, I>> C,
  std::optional<VecRef<T>> u,
  std::optional<VecRef<T>> l,
  std::optional<VecRef<T>> x = std::nullopt,
  std::optional<VecRef<T>> y = std::nullopt,
  std::optional<VecRef<T>> z = std::nullopt,
  std::optional<T> eps_abs = std::nullopt,
  std::optional<T> eps_rel = std::nullopt,
  std::optional<T> rho = std::nullopt,
  std::optional<T> mu_eq = std::nullopt,
  std::optional<T> mu_in = std::nullopt,
  std::optional<bool> verbose = std::nullopt,
  bool compute_preconditioner = true,
  bool compute_timings = true,
  std::optional<isize> max_iter = std::nullopt,
  proxsuite::proxqp::InitialGuessStatus initial_guess =
    proxsuite::proxqp::InitialGuessStatus::EQUALITY_CONSTRAINED_INITIAL_GUESS)
{

  isize n(0);
  isize n_eq(0);
  isize n_in(0);
  if (H != std::nullopt) {
    n = H.value().rows();
  }
  if (A != std::nullopt) {
    n_eq = A.value().rows();
  }
  if (C != std::nullopt) {
    n_in = C.value().rows();
  }

  proxqp::sparse::QP<T, I> Qp(n, n_eq, n_in);
  Qp.settings.initial_guess = initial_guess;

  if (eps_abs != std::nullopt) {
    Qp.settings.eps_abs = eps_abs.value();
  }
  if (eps_rel != std::nullopt) {
    Qp.settings.eps_rel = eps_rel.value();
  }
  if (verbose != std::nullopt) {
    Qp.settings.verbose = verbose.value();
  }
  if (max_iter != std::nullopt) {
    Qp.settings.max_iter = verbose.value();
  }
  Qp.settings.compute_timings = compute_timings;
  Qp.init(H, g, A, b, C, u, l, compute_preconditioner, rho, mu_eq, mu_in);
  Qp.solve(x, y, z);

  return Qp.results;
}

} // namespace sparse
} // namespace proxqp
} // namespace proxsuite

#endif /* end of include guard PROXSUITE_QP_SPARSE_WRAPPER_HPP */
