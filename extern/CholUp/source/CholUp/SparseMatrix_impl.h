#pragma once

#include <fstream>
#include <complex>
#include <assert.h>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <iostream>

namespace CholUp {

template<class T>
bool SparseMatrix<T>::compareTriplet(const Triplet& t0, const Triplet& t1)
{

    if(std::get<1>(t0) < std::get<1>(t1)) return true;
    else if(std::get<1>(t0) > std::get<1>(t1)) return false;
    else if(std::get<0>(t0) < std::get<0>(t1)) return true;
    else return false;
}

template<class T>
SparseMatrix<T>::SparseMatrix()
{
}

template<class T>
SparseMatrix<T>::SparseMatrix(const int nrows_, const int ncols_)
: nrows(nrows_), ncols(ncols_)
{
    col = new int[ncols + 1];
    std::fill_n(col, ncols + 1, 0);
}

template<class T>
SparseMatrix<T>::SparseMatrix(SparseMatrix&& A)
{
    *this = A;
}

template<class T>
SparseMatrix<T>::SparseMatrix(const SparseMatrix& A)
{
    *this = A;
}

template<class T>
SparseMatrix<T>::SparseMatrix(Eigen::SparseMatrix<T, Eigen::ColMajor, int>& eigenMatrix)
:   dataBorrowed(true),
    nrows((int)eigenMatrix.rows()),
    nnz((int)eigenMatrix.nonZeros()),
    ncols((int)eigenMatrix.cols()),
    col(eigenMatrix.outerIndexPtr()),
    row(eigenMatrix.innerIndexPtr()),
    vals(eigenMatrix.valuePtr())
{
    setDiagonalIndizes();
}

template<class T>
SparseMatrix<T>::SparseMatrix(const int rows, const int cols, const int nnz_)
: nrows(rows), ncols(cols), nnz(nnz_), col(new int[cols + 1]), row(new int[nnz_]), vals(new T[nnz_])
{

}

template<class T>
SparseMatrix<T>::~SparseMatrix()
{
    if(!dataBorrowed)
    {
        if(col) delete[] col;
        if(row) delete[] row;
        if(vals) delete[] vals;
    }

    if(diag) delete[] diag;
}


template<class T>
Matrix<double>
SparseMatrix<T>::operator*(const Matrix<double>& m) const
{
    if(ncols != m.nrows)
    {
        std::cout << "matrix dimensions do not match" << std::endl;
        return Matrix<double>();
    }

    Matrix<double> ret(nrows, m.ncols);
    ret.fill();

    std::vector<double> buffer(m.ncols);

    for(int i = 0; i < ncols; ++i)
    {
        std::fill_n(buffer.data(), m.ncols, 0);

        for(int k = 0; k < m.ncols; ++k)
        {
            buffer[k] = m(i, k);
        }

        for(int j = col[i]; j < col[i+1]; ++j)
        {
            for(int k = 0; k < m.ncols; ++k)
            {
                ret(row[j], k) += buffer[k] * vals[j];
            }
        }
    }

    return ret;
}



template<class T>
SparseMatrix<T>& SparseMatrix<T>::operator=(const SparseMatrix& A)
{
    std::cout << "warning: explicit sparse matrix copy." << std::endl;

    nnz = A.nnz;
    ncols = A.ncols;
    nrows = A.nrows;

    row = new int[nnz];
    std::copy_n(A.row, nnz, row);

    col = new int[ncols + 1];
    std::copy_n(A.col, ncols + 1, col);

    vals = new T[nnz];
    std::copy_n(A.vals, nnz, vals);

    if(A.diag)
    {
        diag = new int[std::min(ncols, nrows)];
        std::copy_n(A.diag, std::min(ncols, nrows), diag);
    }

    return *this;
}


template<class T>
SparseMatrix<T>& SparseMatrix<T>::operator=(SparseMatrix&& A)
{
    dataBorrowed = A.dataBorrowed;

    nnz = A.nnz;
    ncols = A.ncols;
    nrows = A.nrows;



    row = A.row;
    col = A.col;
    vals = A.vals;
    nrows = A.nrows;
    diag = A.diag;

    A.row = A.col = A.diag = nullptr;
    A.vals = nullptr;

    return *this;
}


template<class T>
void
SparseMatrix<T>::setDiagonalIndizes()
{
    if(diag) delete [] diag;
    diag = new int[ncols];
    std::fill_n(diag, ncols, -1);

    for(int i = 0; i < ncols; ++i)
    {
        for(int j = col[i]; j < col[i+1]; ++j)
        {
            if(row[j] == i)
            {
                diag[i] = j;
                break;
            }
        }
    }
}

template<class T>
void SparseMatrix<T>::addToDiagonal(const T val)
{
    for(int i = 0; i < ncols; ++i)
    {
        for(int j = col[i]; j < col[i+1]; ++j)
        {
            if(row[j] == i)
            {
                vals[j] += val;
                break;
            }
        }

    }
}

template<class T>
void
SparseMatrix<T>::setTriplets(std::vector<Triplet>& triplets, int ncols)
{

    assert(std::all_of(triplets.begin(), triplets.end(), [=](const Triplet& t){return ncols > std::get<2>(t);}));
    std::sort(triplets.begin(), triplets.end(), compareTriplet);

    if(col) delete[] col;
    col = new int [ncols + 1];

    int nt = triplets.size();

    if(row) delete[] row;
    row = new int[nt];

    if(vals) delete[] vals;
    vals = new T[nt];

    int cnt = 0;

    while(cnt <= std::get<1>(triplets.front()))
        col[cnt++] = 0;

    int i = std::get<0>(triplets.front());
    int j = std::get<1>(triplets.front());
    T sum = T();

    cnt = 0;
    int colCnt = 0;

    for(auto& t : triplets)
    {
        if(std::get<0>(t) == i && std::get<1>(t) == j)
        {
            sum += std::get<2>(t);
        } else
        {
            row[cnt] = i;
            vals[cnt] = sum;

            ++cnt;

            while(std::get<1>(t) > j++)
                col[colCnt++] = cnt;

            tie(i, j, sum) = t;
        }
    }

    row[cnt] = i;
    vals[cnt] = sum;

    ++cnt;

    while(j >= ncols)
    {
        col[colCnt++] = cnt;
    }

    col[colCnt++] = cnt;
}

template<class T>
void
SparseMatrix<T>::writeMatrixMarket(const std::string& filename, const bool symmetric) const
{

    std::ofstream file(filename);
    file << "%%MatrixMarket matrix coordinate real ";

    if(symmetric) file << "symmetric";
    else file << "general";

    file << std::endl;
    file << std::setiosflags(std::ios::fixed);
    file.precision(20);

    if(!nnz || !ncols)
    {
        file.close();
        return;
    }

    const int nrows2 = nrows == -1 ? ncols : nrows;

    file << nrows2 << " " << ncols << " " << nnz << "\n";
    for(int i = 0; i < ncols; ++i)
        for(int j = col[i]; j < col[i+1]; ++j)
            file << row[j] + 1 << " " << i + 1 << " " << vals[j] << "\n";

    file.close();
}

} /* namespace CholUp */
