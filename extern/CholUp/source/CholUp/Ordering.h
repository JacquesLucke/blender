#pragma once

#include <Eigen/Sparse>
#include "SparseMatrix.h"
#include "Timer.hpp"

namespace CholUp {

template<class T>
CholUp::SparseMatrix<T>
buildPermutedMatrix(const Eigen::SparseMatrix<T>& A, std::vector<int>& perm)
{
    assert(A.isCompressed());
    assert(A.cols() == A.rows());

    const int N = (int)A.cols();

    // initialize permuted matrix
    CholUp::SparseMatrix<T> ret(N, N, (int)A.nonZeros());

    // col sizes
    for(int i = 0; i < N; ++i)
    {
        ret.col[perm[i]] = A.outerIndexPtr()[i + 1] - A.outerIndexPtr()[i];
    }

    // sumup to go from sizes to start indizes
    int sum = 0;
    int tmp = 0;

    for(int i = 0; i < N + 1; ++i)
    {
        tmp = sum;
        std::swap(ret.col[i], tmp);
        sum += tmp;
    }

    // fill columns of 'ret' by permuted and sort columns of 'A'
    std::pair<int, double>* buffer = new std::pair<int, double>[N];

    for(int i = 0; i < N; ++i)
    {
        auto ptr = buffer;

        for(int j = A.outerIndexPtr()[i]; j < A.outerIndexPtr()[i + 1]; ++j)
        {
            *ptr++ = std::make_pair(perm[A.innerIndexPtr()[j]], A.valuePtr()[j]);
        }

        std::sort(buffer, ptr);

        const int newColumn = perm[i];
        int idx = ret.col[newColumn];

        for(auto it = buffer; it != ptr; ++it, ++idx)
        {
            ret.row[idx] = it->first;
            ret.vals[idx] = it->second;
        }

        assert(idx == ret.col[newColumn + 1]);
    }

    delete[] buffer;

    ret.setDiagonalIndizes();

    return ret;
}

template<class T>
CholUp::SparseMatrix<T>
permuteMatrix(const Eigen::SparseMatrix<T>& A, std::vector<int>& perm)
{
    Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic, int> p;
    Eigen::SparseMatrix<T> B = A;

    Eigen::internal::minimum_degree_ordering(B, p);

    perm.resize(A.rows());

    for(int i = 0; i < A.rows(); ++i)
    {
        perm[p.indices()[i]] = i;
    }

    return buildPermutedMatrix(A, perm);
}

} /* namespace CholUp */
