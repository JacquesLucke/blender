#pragma once

namespace CholUp {

template<class T, int Cols>
class SupernodalVector
{
public:
    int NR; // number of non-zero rows
    int NNZ; // number of non-zeros

    int* rows = 0;
    T* vals = 0;

    explicit SupernodalVector(const int NR_ = 0);

    SupernodalVector(SupernodalVector&& A);
    SupernodalVector(const SupernodalVector& A);

    SupernodalVector& operator=(SupernodalVector&& A);
    SupernodalVector& operator=(const SupernodalVector& A);

    ~SupernodalVector();
};


template<class T>
class SparseSupernodalMatrix
{
public:

    int numrows = -1;
    int numcols; // number of columns (= total(supernodeSizes) )

    int NS = 0; // number of supernodes, aka 'cols' has NS + 1 entries.
    int NR = 0; // number of entries in 'rows'.
    int NNZ = 0; // number of entries in 'vals'.

    int* colMap = nullptr;
    int* supernodeSizes = nullptr;
    int* snodeValueStart = nullptr;
    int* cols = nullptr;
    int* rows = nullptr;
    double* vals = nullptr;

    ~SparseSupernodalMatrix();

    SparseSupernodalMatrix();
    SparseSupernodalMatrix(SparseSupernodalMatrix&& A);
    SparseSupernodalMatrix(const SparseSupernodalMatrix& A);

    SparseSupernodalMatrix& operator=(SparseSupernodalMatrix&& A);
    SparseSupernodalMatrix& operator=(const SparseSupernodalMatrix& A);

    SparseMatrix<T> toSparseMatrix(const bool moveVals = false, const bool transposed = false);
};

////// Implementation /////

template<class T, int Cols>
SupernodalVector<T, Cols>::SupernodalVector(SupernodalVector&& A)
{
    *this = std::move(A);
}

template<class T, int Cols>
SupernodalVector<T, Cols>::SupernodalVector(const SupernodalVector& A)
{
    *this = A;
}

template<class T, int Cols>
SupernodalVector<T, Cols>&
SupernodalVector<T, Cols>::operator=(SupernodalVector&& A)
{
    if(this != &A)
    {
        NR = A.NR;
        NNZ = A.NNZ;

        rows = A.rows;
        vals = A.vals;

        A.rows = nullptr;
        A.vals = nullptr;
    }

    return *this;
}

template<class T, int Cols>
SupernodalVector<T, Cols>&
SupernodalVector<T, Cols>::operator=(const SupernodalVector& A)
{
    if(this != &A)
    {
        NR = A.NR;
        NNZ = A.NNZ;

        rows = new int[NR];
        std::copy_n(A.rows, NR, rows);

        vals = new T[NNZ];
        std::copy_n(A.vals, NNZ, vals);
    }

    return *this;
}


template<class T, int Cols>
SupernodalVector<T, Cols>::SupernodalVector(const int NR_)
: NR(NR_), NNZ(NR_ * Cols)
{
    if(NR)
    {
        rows = new int[NR];
        vals = new T[NNZ];
    }
}

template<class T, int Cols>
SupernodalVector<T, Cols>::~SupernodalVector()
{
    if(rows) delete[] rows;
    if(vals) delete[] vals;
}


template<class T>
SparseSupernodalMatrix<T>::SparseSupernodalMatrix(const SparseSupernodalMatrix& A)
{
    *this = A;
}

template<class T>
SparseSupernodalMatrix<T>&
SparseSupernodalMatrix<T>::operator=(const SparseSupernodalMatrix& A)
{
    if(&A != this)
    {
        numrows = A.numrows;
        numcols = A.numcols;

        NR = A.NR;
        NS = A.NS;
        NNZ = A.NNZ;

        supernodeSizes = new int[NS];
        std::copy_n(A.supernodeSizes, NS, supernodeSizes);

        snodeValueStart = new int[NS];
        std::copy_n(A.snodeValueStart, NS, snodeValueStart);

        cols = new int[NS + 1];
        std::copy_n(A.cols, NS + 1, cols);

        rows = new int[NR];
        std::copy_n(A.rows, NR, rows);

        vals = new T[NNZ];
        std::copy_n(A.vals, NNZ, vals);

        colMap = new int[numcols];
        std::copy_n(A.colMap, numcols, colMap);
    }

    return *this;
}


template<class T>
SparseSupernodalMatrix<T>::SparseSupernodalMatrix(SparseSupernodalMatrix&& A)
{
    *this = std::move(A);
}

template<class T>
SparseSupernodalMatrix<T>&
SparseSupernodalMatrix<T>::operator=(SparseSupernodalMatrix&& A)
{
    if(&A != this)
    {
        numrows = A.numrows;
        numcols = A.numcols;

        NR = A.NR;
        NS = A.NS;
        NNZ = A.NNZ;

        colMap = A.colMap;
        supernodeSizes = A.supernodeSizes;
        snodeValueStart = A.snodeValueStart;
        cols = A.cols;
        rows = A.rows;
        vals = A.vals;

        A.colMap = nullptr;
        A.supernodeSizes = nullptr;
        A.snodeValueStart = nullptr;
        A.cols = nullptr;
        A.rows = nullptr;
        A.vals = nullptr;
    }

    return *this;
}

template<class T>
SparseSupernodalMatrix<T>::~SparseSupernodalMatrix()
{
    if(vals) delete[] vals;
    if(rows) delete[] rows;
    if(cols) delete[] cols;
    if(supernodeSizes) delete[] supernodeSizes;
    if(snodeValueStart) delete[] snodeValueStart;
    if(colMap) delete[] colMap;
}

template<class T>
SparseSupernodalMatrix<T>::SparseSupernodalMatrix()
{

}


template<class T>
SparseMatrix<T>
SparseSupernodalMatrix<T>::toSparseMatrix(const bool moveVals, const bool transposed)
{
    SparseMatrix<T> ret;

    if(transposed)
    {
        ret.col = new int[numrows + 1];
        std::fill_n(ret.col, numrows + 1, 0);

        ret.nrows = numcols;
        ret.ncols = numrows;
        ret.nnz = NNZ;

        for(int i = 0; i < NS; ++i)
        {
            const int ss = supernodeSizes[i];

            for(int j = cols[i]; j < cols[i+1]; ++j)
            {
                ret.col[rows[j]] += ss;
            }
        }

        double sum = .0;

        for(int i = 0; i <= numrows; ++i)
        {
            double tmp = ret.col[i];
            ret.col[i] = sum;
            sum += tmp;
        }

        ret.vals = new T[ret.col[numrows]];
        ret.row = new int[ret.col[numrows]];

        // copy values
        int k0 = 0;

        for(int i = 0; i < NS; ++i)
        {
            double* vptr = &vals[snodeValueStart[i]];
            const int ss = supernodeSizes[i];
            const int nr = cols[i+1] - cols[i];

            for(int j = cols[i]; j < cols[i+1]; ++j)
            {
                for(int k = 0; k < ss; ++k)
                {
                    const int id = ret.col[rows[j]]++;

                    assert(id < ret.col[numrows]);

                    ret.vals[id] = *(vptr + k * nr);
                    ret.row[id] = k0 + k;
                }

                ++vptr;
            }

            k0 += ss;
        }



        for(int i = numrows; i > 0; --i)
            ret.col[i] = ret.col[i-1];

        ret.col[0] = 0;

    } else
    {
        ret.col = new int[numcols + 1];
        ret.col[0] = 0;

        ret.nrows = numrows;
        ret.ncols = numcols;
        ret.nnz = NNZ;

        ret.row = new int[NNZ];
        ret.vals = new T[NNZ];

        std::copy(vals, vals + NNZ, ret.vals);

        int cnt = 0;

        int k0 = 0;
        for(int i = 0; i < NS; ++i)
        {
            int ss = supernodeSizes[i];

            for(int k = 0; k < ss; ++k)
            {
                // copy indizes of column k in supernode i
                for(int j = cols[i]; j < cols[i+1]; ++j)
                {
                    ret.row[cnt++] = rows[j];
                }

                ret.col[k0 + 1] = ret.col[k0] + (cols[i+1] - cols[i]);
                ++k0;
            }
        }
    }

    return ret;
}

} /* namespace CholUp */