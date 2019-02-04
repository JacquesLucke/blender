#pragma once

#include <vector>
#include <tuple>
#include <string>
#include <Eigen/Sparse>
#include "Matrix.hpp"

namespace CholUp {

template<class T>
class SparseMatrix
{
public:
    typedef std::tuple<int, int, T> Triplet;
    static bool compareTriplet(const Triplet& t0, const Triplet& t1);

    typedef T ValueType;

    bool dataBorrowed = false;

    int nrows = -1;
    int nnz = 0;
    int ncols = 0;

    int* col = nullptr;
    int* diag = nullptr;
    int* row = nullptr;
    T* vals = nullptr;

    SparseMatrix();
    SparseMatrix(const int nrows, const int ncols);
    explicit SparseMatrix(const SparseMatrix& A);
    SparseMatrix(SparseMatrix&& A);
    SparseMatrix& operator=(SparseMatrix&& A);
    SparseMatrix& operator=(const SparseMatrix& A);

    explicit SparseMatrix(Eigen::SparseMatrix<T, Eigen::ColMajor, int>& eigenMatrix);
    SparseMatrix(const int cols, const int rows, const int nnz);

    void setDiagonalIndizes();

    ~SparseMatrix();

    CholUp::Matrix<double> operator*(const CholUp::Matrix<double>& m) const;

    void setTriplets(std::vector<Triplet>& triplets, int ncols);

    static SparseMatrix identity(const int n);

    void addToDiagonal(const T val);

    void writeMatrixMarket(const std::string& filename, const bool symmetric = false) const;
};

template<class T>
SparseMatrix<T> fromEigen(const Eigen::SparseMatrix<T>& A);

template<class T>
SparseMatrix<T> fromEigen(const Eigen::SparseMatrix<T>& A)
{
    SparseMatrix<T> ret;

    ret.nnz = (int)A.nonZeros();
    ret.nrows = (int)A.rows();
    ret.ncols = (int)A.cols();

    ret.col = new int[A.cols() + 1];
    ret.row = new int[A.nonZeros()];
    ret.vals = new T[A.nonZeros()];

    std::copy(A.outerIndexPtr(), A.outerIndexPtr() + A.cols() + 1, ret.col);
    std::copy(A.innerIndexPtr(), A.innerIndexPtr() + A.nonZeros(), ret.row);
    std::copy(A.valuePtr(), A.valuePtr() + A.nonZeros(), ret.vals);

    ret.setDiagonalIndizes();

    return std::move(ret);
}

} /* namespace CholUp */

#include "SparseMatrix_impl.h"
