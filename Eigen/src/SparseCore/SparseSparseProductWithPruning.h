// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_SPARSESPARSEPRODUCTWITHPRUNING_H
#define EIGEN_SPARSESPARSEPRODUCTWITHPRUNING_H

namespace internal {


// perform a pseudo in-place sparse * sparse product assuming all matrices are col major
template<typename Lhs, typename Rhs, typename ResultType>
static void sparse_sparse_product_with_pruning_impl(const Lhs& lhs, const Rhs& rhs, ResultType& res, typename ResultType::RealScalar tolerance)
{
  // return sparse_sparse_product_with_pruning_impl2(lhs,rhs,res);

  typedef typename remove_all<Lhs>::type::Scalar Scalar;
  typedef typename remove_all<Lhs>::type::Index Index;

  // make sure to call innerSize/outerSize since we fake the storage order.
  Index rows = lhs.innerSize();
  Index cols = rhs.outerSize();
  //int size = lhs.outerSize();
  eigen_assert(lhs.outerSize() == rhs.innerSize());

  // allocate a temporary buffer
  AmbiVector<Scalar,Index> tempVector(rows);

  // estimate the number of non zero entries
  float ratioLhs = float(lhs.nonZeros())/(float(lhs.rows())*float(lhs.cols()));
  float avgNnzPerRhsColumn = float(rhs.nonZeros())/float(cols);
  float ratioRes = (std::min)(ratioLhs * avgNnzPerRhsColumn, 1.f);

  // mimics a resizeByInnerOuter:
  if(ResultType::IsRowMajor)
    res.resize(cols, rows);
  else
    res.resize(rows, cols);

  res.reserve(Index(ratioRes*rows*cols));
  for (Index j=0; j<cols; ++j)
  {
    // let's do a more accurate determination of the nnz ratio for the current column j of res
    //float ratioColRes = (std::min)(ratioLhs * rhs.innerNonZeros(j), 1.f);
    // FIXME find a nice way to get the number of nonzeros of a sub matrix (here an inner vector)
    float ratioColRes = ratioRes;
    tempVector.init(ratioColRes);
    tempVector.setZero();
    for (typename Rhs::InnerIterator rhsIt(rhs, j); rhsIt; ++rhsIt)
    {
      // FIXME should be written like this: tmp += rhsIt.value() * lhs.col(rhsIt.index())
      tempVector.restart();
      Scalar x = rhsIt.value();
      for (typename Lhs::InnerIterator lhsIt(lhs, rhsIt.index()); lhsIt; ++lhsIt)
      {
        tempVector.coeffRef(lhsIt.index()) += lhsIt.value() * x;
      }
    }
    res.startVec(j);
    for (typename AmbiVector<Scalar,Index>::Iterator it(tempVector,tolerance); it; ++it)
      res.insertBackByOuterInner(j,it.index()) = it.value();
  }
  res.finalize();
}

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = traits<Lhs>::Flags&RowMajorBit,
  int RhsStorageOrder = traits<Rhs>::Flags&RowMajorBit,
  int ResStorageOrder = traits<ResultType>::Flags&RowMajorBit>
struct sparse_sparse_product_with_pruning_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;
  typedef typename ResultType::RealScalar RealScalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, RealScalar tolerance)
  {
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    sparse_sparse_product_with_pruning_impl<Lhs,Rhs,ResultType>(lhs, rhs, _res, tolerance);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, RealScalar tolerance)
  {
    // we need a col-major matrix to hold the result
    typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
    SparseTemporaryType _res(res.rows(), res.cols());
    sparse_sparse_product_with_pruning_impl<Lhs,Rhs,SparseTemporaryType>(lhs, rhs, _res, tolerance);
    res = _res;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, RealScalar tolerance)
  {
    // let's transpose the product to get a column x column product
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    sparse_sparse_product_with_pruning_impl<Rhs,Lhs,ResultType>(rhs, lhs, _res, tolerance);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, RealScalar tolerance)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix colLhs(lhs);
    ColMajorMatrix colRhs(rhs);
    sparse_sparse_product_with_pruning_impl<ColMajorMatrix,ColMajorMatrix,ResultType>(colLhs, colRhs, res, tolerance);

    // let's transpose the product to get a column x column product
//     typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
//     SparseTemporaryType _res(res.cols(), res.rows());
//     sparse_sparse_product_with_pruning_impl<Rhs,Lhs,SparseTemporaryType>(rhs, lhs, _res);
//     res = _res.transpose();
  }
};

// NOTE the 2 others cases (col row *) must never occur since they are caught
// by ProductReturnType which transforms it to (col col *) by evaluating rhs.

} // end namespace internal

#endif // EIGEN_SPARSESPARSEPRODUCTWITHPRUNING_H