#ifndef dplyr_hybrid_min_max_h
#define dplyr_hybrid_min_max_h

#include <dplyr/hybrid/HybridVectorScalarResult.h>
#include <dplyr/hybrid/Column.h>
#include <tools/default_value.h>
#include <dplyr/hybrid/Expression.h>

namespace dplyr {
namespace hybrid {

namespace internal {

template <int RTYPE, typename SlicedTibble, bool MINIMUM, bool NA_RM>
class MinMax : public HybridVectorScalarResult<REALSXP, SlicedTibble, MinMax<RTYPE, SlicedTibble, MINIMUM, NA_RM> > {
public:
  typedef HybridVectorScalarResult<REALSXP, SlicedTibble, MinMax> Parent ;
  typedef typename Rcpp::Vector<RTYPE>::stored_type STORAGE;

  MinMax(const SlicedTibble& data, Column column_, SEXP expr_):
    Parent(data),
    column(column_.data),
    warn(false),
    expr(expr_)
  {}

  ~MinMax() {
    if (NA_RM && warn) {
      if (MINIMUM) {
        Rf_warningcall(expr, "no non-missing arguments to min; returning Inf");
      } else {
        Rf_warningcall(expr, "no non-missing arguments to max; returning -Inf");
      }
    }
  }

  inline double process(const typename SlicedTibble::slicing_index& indices) const {
    const int n = indices.size();
    double res = Inf;
    int missing_count = 0;

    for (int i = 0; i < n; ++i) {
      STORAGE current = column[indices[i]];

      if (is_really_na<RTYPE>(current)) {
        if (NA_RM) {
          ++missing_count;
          continue;
        } else {
          return NA_REAL;
        }

      }
      else if (is_nan<RTYPE>(current)) {
        return current;
      } else {
        double current_res = current;
        if (is_better(current_res, res))
          res = current_res;
      }
    }
    if (missing_count == n) {
      warn = true;
    }
    return res;
  }

private:
  Rcpp::Vector<RTYPE> column;
  mutable bool warn;
  SEXP expr;

  static const double Inf;

  inline static bool is_better(const double current, const double res) {
    if (MINIMUM)
      return current < res;
    else
      return res < current ;
  }
};

template <int RTYPE, typename SlicedTibble, bool MINIMUM, bool NA_RM>
const double MinMax<RTYPE, SlicedTibble, MINIMUM, NA_RM>::Inf = (MINIMUM ? R_PosInf : R_NegInf);

}

// min( <column> )
template <typename SlicedTibble, typename Operation, bool MINIMUM, bool NARM>
SEXP minmax_narm(const SlicedTibble& data, Column x, const Operation& op, SEXP expr) {

  // only handle basic number types, anything else goes through R
  switch (TYPEOF(x.data)) {
  case RAWSXP:
    return op(internal::MinMax<RAWSXP, SlicedTibble, MINIMUM, NARM>(data, x, expr));
  case INTSXP:
    return op(internal::MinMax<INTSXP, SlicedTibble, MINIMUM, NARM>(data, x, expr));
  case REALSXP:
    return op(internal::MinMax<REALSXP, SlicedTibble, MINIMUM, NARM>(data, x, expr));
  default:
    break;
  }

  return R_UnboundValue;
}

template <typename SlicedTibble, typename Operation, bool MINIMUM>
SEXP minmax_(const SlicedTibble& data, Column x, bool narm, const Operation& op, SEXP expr) {
  if (narm) {
    return minmax_narm<SlicedTibble, Operation, MINIMUM, true>(data, x, op, expr) ;
  } else {
    return minmax_narm<SlicedTibble, Operation, MINIMUM, false>(data, x, op, expr) ;
  }
}

template <typename SlicedTibble, typename Operation, bool MINIMUM>
SEXP minmax_dispatch(const SlicedTibble& data, const Expression<SlicedTibble>& expression, const Operation& op) {
  Column x;
  switch (expression.size()) {
  case 1:
    // min( <column> )
    if (expression.is_unnamed(0) && expression.is_column(0, x)) {
      return minmax_<SlicedTibble, Operation, MINIMUM>(data, x, false, op, expression.get_expr()) ;
    }
  case 2:
    // min( <column>, na.rm = <bool> )
    bool test;

    if (expression.is_unnamed(0) && expression.is_column(0, x) && expression.is_named(1, symbols::narm) && expression.is_scalar_logical(1, test)) {
      return minmax_<SlicedTibble, Operation, MINIMUM>(data, x, test, op, expression.get_expr()) ;
    }
  default:
    break;
  }
  return R_UnboundValue;
}

template <typename SlicedTibble, typename Operation>
SEXP min_dispatch(const SlicedTibble& data, const Expression<SlicedTibble>& expression, const Operation& op) {
  return minmax_dispatch<SlicedTibble, Operation, true>(data, expression, op);
}

template <typename SlicedTibble, typename Operation>
SEXP max_dispatch(const SlicedTibble& data, const Expression<SlicedTibble>& expression, const Operation& op) {
  return minmax_dispatch<SlicedTibble, Operation, false>(data, expression, op);
}

}
}

#endif
