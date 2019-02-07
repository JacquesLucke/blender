#pragma once

#include "SupernodalCholesky.h"
#include "SparseMatrix.h"
#include "Matrix.hpp"
#include "EliminationTreeMethods.h"
#include "SparseSupernodal.h"
#include "Ordering.h"
#include "Timer.hpp"
#include <vector>
#include <algorithm>
#include <array>
#include <thread>
#include <cstring>

namespace CholUp {

    template<class MatrixType>
    class SupernodalCholesky
    {
    public:

        std::vector<int> perm, iperm;
        SparseMatrix<double> A; // original matrix

        typedef typename MatrixType::ValueType T;

        constexpr static int NUMTHREADS = 1;

        int N;
        int *etree = nullptr, *setree = nullptr;

        int* startColsInRow = nullptr;
        int* colsInRow = nullptr;
        int* colsInRowColIndex = nullptr;

        int* dirtyNodes = nullptr; // nodes to be updated
        int topDirtyNodes = 0; // index of first update node

        // workspaces
        std::vector<int> flag;
        std::vector<int> rowMap; // length N, default: -1, do we need this or can another array be used?

        int wslen = 0;
        double *ws = nullptr;

        int* iwsN = nullptr;
        int* iwsN2 = nullptr;

        SparseSupernodalMatrix<T> L;

        SupernodalCholesky<MatrixType>
        dirichletPartialFactor(const std::vector<int>& roi);

        void
        subfactor(const std::vector<int>& scols,
                  const std::vector<int>& rowMap,
                  const std::vector<int>& updateSeeds,
                  SupernodalCholesky<MatrixType>& subfactor,
                  int* stats = 0);

        std::vector<int> findUpdateColumns(const std::vector<int>& rowMap, const int NROI);

        void initWorkspace(const int len);

        void initWorkspace();

        void symbolic(const MatrixType& A);

        void numeric(const MatrixType& A);

        void solveL(Matrix<T>& m);

        void solveLT(Matrix<T>& m);

        void solve(Matrix<T>& m);

        void solve3_RowMajor(T* md);

        template<int Cols>
        void solveL_RowMajor(T*);

        template<int Cols>
        void solveLT_RowMajor(T*);

        std::string memoryReport();

        void update(SparseMatrix<double>& W);

        int getDependantSupernodes(const std::vector<int>& nodes, int* out);

        void copySupernodes(const std::vector<int>& scols,
                            const std::vector<int>& rowMap,
                            const int* skipStart, const int* skipEnd,
                            SupernodalCholesky<MatrixType>& subfactor);



        void partialRefactorize(const MatrixType& A0,
                                const std::vector<int>& subCols,
                                const std::vector<int>& rowMapA0,
                                int* flag_ = nullptr,
                                int* iws_ = nullptr,
                                double* ws_ = nullptr,
                                int wslen_ = 0);

        SparseSupernodalMatrix<T>
        solveL(const SparseSupernodalMatrix<T>& m);

        ~SupernodalCholesky();

        SupernodalCholesky() {};

        explicit SupernodalCholesky(const Eigen::SparseMatrix<double, Eigen::ColMajor, int>& A);

        explicit SupernodalCholesky(const MatrixType& A);

        SupernodalCholesky(SupernodalCholesky&& A);

        SupernodalCholesky&
        operator=(SupernodalCholesky<MatrixType>&& A);

    };


    template<class MatrixType>
    std::string
    SupernodalCholesky<MatrixType>::memoryReport()
    {

        const int matrixStructureData = (3 * L.NS + L.NR + L.numcols) * sizeof(int);
        const int matrixValueData = L.NNZ * sizeof(T);
        const int rowStructureData = (L.numcols + 1 + 2 * L.NR) * sizeof(int);
        const int treeData = (L.NS + L.numcols) * sizeof(int);
        const int workspaceData = wslen * sizeof(double);
        const int workspaceTmpData = 2 * L.numcols * sizeof(int);

        return std::string("Memory report: \n") +
        std::string("factor structure data    : ") + std::to_string(matrixStructureData) + "\n" +
        "factor value data        : " + std::to_string(matrixValueData) + "\n" +
        "row structure data       : " + std::to_string(rowStructureData) + "\n" +
        "tree information         : " + std::to_string(treeData) + "\n" +
        "total factorization data : " + std::to_string( (matrixStructureData + matrixValueData + rowStructureData + treeData) * 10e-6 ) + " MB \n" +
        "workspace data           : " + std::to_string(workspaceData) + "\n" +
        "temporary ws data        : " + std::to_string(workspaceTmpData) + "\n\n";
    }

    template<class MatrixType>
    SupernodalCholesky<MatrixType>::SupernodalCholesky(const Eigen::SparseMatrix<double>& A0)
    : N(A0.cols()), flag(A0.cols(), 0), rowMap(A0.cols(), -1)
    {
     //   Timer t;

        A = permuteMatrix(A0, perm);

        iperm.resize(perm.size());

        for(int i = 0; i < A.ncols; ++i)
            iperm[perm[i]] = i;

     //   t.printTime("permute");
      //  t.reset();
        symbolic(A);
      //  t.printTime("symbolic");
      //  t.reset();
        numeric(A);
    //    t.printTime("numeric");

    }

    template<class MatrixType>
    SupernodalCholesky<MatrixType>::~SupernodalCholesky()
    {
        if(etree) delete[] etree;
        if(setree) delete[] setree;
        if(ws) delete[] ws;
        if(iwsN) delete[] iwsN;
        if(iwsN2) delete[] iwsN2;

        if(dirtyNodes) delete[] dirtyNodes;
        if(startColsInRow) delete[] startColsInRow;
        if(colsInRow) delete[] colsInRow;
        if(colsInRowColIndex) delete[] colsInRowColIndex;
    }

    extern "C"
    {
        void dgemm_ (char *transa, char *transb, int *m, int *n,
                     int *k, double *alpha, double *A, int *lda, double *B,
                     int *ldb, double *beta, double *C, int *ldc) ;

        void dpotrf_ (char *uplo, int *n, double *A, int *lda, int *info) ;

        void dtrsm_ (char* side, char* uplo, char* transa, char* diag, int* m, int* n, double* alpha,
                     double* A, int* lda, double* B, int* ldb);

        void dsyrk_ (char* uplo, char* trans, int* n, int* k, double* alpha, double* A,
                     int* lda, double* beta, double* C, int* ldc);

        void dcopy_(int*, double*, int*, double*, int*);
    }

    namespace
    {
        double one = 1.;
        double zero = .0;
        double minus_one = -1.;
        int info;

        char cL = 'L';
        char cN = 'N';
        char cT = 'T';
        char cR = 'R';
    }


    template<class MatrixType>
    SupernodalCholesky<MatrixType>&
    SupernodalCholesky<MatrixType>::operator=(SupernodalCholesky<MatrixType>&& A)
    {
        if(&A != this)
        {
            N = A.N;
            L = std::move(A.L);
            flag = std::move(A.flag);
            rowMap = std::move(A.rowMap);
            perm = std::move(A.perm);
            iperm = std::move(A.iperm);

            setree = A.setree;
            A.setree = nullptr;

            etree = A.etree;
            A.setree = nullptr;

            startColsInRow = A.startColsInRow;
            A.startColsInRow = nullptr;

            startColsInRow = A.colsInRow;
            A.colsInRow = nullptr;

            startColsInRow = A.colsInRowColIndex;
            A.colsInRowColIndex = nullptr;

            dirtyNodes = A.dirtyNodes;
            A.dirtyNodes = nullptr;

            topDirtyNodes = A.topDirtyNodes;

            wslen = A.wslen;
            wslen = 0;

            ws = A.ws;
            A.ws = nullptr;

            iwsN = A.iwsN;
            A.iwsN = nullptr;

            iwsN2 = A.iwsN2;
            A.iwsN2 = nullptr;
        }

        return *this;
    }

    template<class MatrixType>
    SupernodalCholesky<MatrixType>::SupernodalCholesky(SupernodalCholesky&& A)
    {
        *this = std::move(A);
    }

    template<class MatrixType>
    void
    SupernodalCholesky<MatrixType>::initWorkspace(const int len)
    {
        if(ws) delete[] ws;
        ws = new double[len];
        std::fill_n(ws, len, 0.);

        wslen = len;
    }

    template<class MatrixType>
    void
    SupernodalCholesky<MatrixType>::initWorkspace()
    {
        initWorkspace(L.numcols);
    }

    template<class MatrixType>
    SupernodalCholesky<MatrixType>
    SupernodalCholesky<MatrixType>::dirichletPartialFactor(const std::vector<int>& roiIds)
    {
        std::vector<int> sortedRoiIds;

        const auto NR = roiIds.size();

        if(!perm.empty())
        {
            sortedRoiIds.reserve(NR);
            for(auto& i : roiIds) sortedRoiIds.push_back(perm[i]);
            std::sort(sortedRoiIds.begin(), sortedRoiIds.end());
        } else
        {
            sortedRoiIds.resize(NR);
            std::partial_sort_copy(roiIds.begin(), roiIds.end(), sortedRoiIds.begin(), sortedRoiIds.end());
        }

        assert(std::is_sorted(sortedRoiIds.begin(), sortedRoiIds.end()));
        assert(std::adjacent_find(sortedRoiIds.begin(), sortedRoiIds.end()) == sortedRoiIds.end());
        assert(std::all_of(rowMap.begin(), rowMap.end(), [](const int i){return i == -1;}));

        for(int i = 0; i < sortedRoiIds.size(); ++i)
            rowMap[sortedRoiIds[i]] = i;

        // identify columns starting an update
        auto updateColumns = findUpdateColumns(rowMap, (int)NR);

        // extract subfactor
        SupernodalCholesky<SparseMatrix<double>> cholPart;
        subfactor(sortedRoiIds, rowMap, updateColumns, cholPart);

        // partial refactorize
        cholPart.partialRefactorize(A, sortedRoiIds, rowMap,
                                    flag.data(),
                                    iwsN,
                                    ws,
                                    wslen);

        // initialize permutation array of cholPart
        cholPart.iperm.resize(NR);

        for(int i = 0; i < NR; ++i)
            rowMap[sortedRoiIds[i]] = -1;

        for(int i = 0; i < NR; ++i)
            rowMap[roiIds[i]] = i;

        for(int i = 0; i < NR; ++i)
        {
            cholPart.iperm[i] = rowMap[ iperm[ sortedRoiIds[i] ] ];
        }

        for(int i = 0; i < NR; ++i)
            rowMap[roiIds[i]] = -1;

        cholPart.initWorkspace();

        return std::move(cholPart);
    }

    template<class MatrixType>
    std::vector<int>
    SupernodalCholesky<MatrixType>::findUpdateColumns(const std::vector<int>& rowMap, const int NROI)
    {
        std::vector<int> updateColumns;
        updateColumns.reserve(N - NROI);

        int lastSupernode = -1;

        for(int i = 0; i < N; ++i)
        {
            if(rowMap[i] == -1)
            {
                const int is = L.colMap[i];

                if(lastSupernode != is)
                {
                    lastSupernode = is;

                    // dont consider diagonal entry
                    for(int j = L.cols[is] + 1; j < L.cols[is+1]; ++j)
                    {
                        const int idx = rowMap[L.rows[j]];
                        if(idx != -1)
                        {
                            updateColumns.push_back(idx);
                            break;
                        }
                    }
                }
            }
        }

        std::sort(updateColumns.begin(), updateColumns.end());
        updateColumns.erase(unique(updateColumns.begin(), updateColumns.end()), updateColumns.end());

        return  updateColumns;
    }

    template<class MatrixType>
    int SupernodalCholesky<MatrixType>::getDependantSupernodes(const std::vector<int>& nodes, int* out)
    {
        const int NS = L.NS;

        std::vector<int> flag(NS, true);

        int top = NS;
        for(int i : nodes)
        {
            i = L.colMap[i];

            int len = 0;
            for(; i != -1 && flag[i]; i = setree[i])
            {
                out[len++] = i;
                flag[i] = false;
            }

            while(len > 0) out[--top] = out[--len];
        }

        return top;
    }

    // refactorize only columns reachable starting from 'leadingColumns'
    // copy original sparse entries directly
    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::partialRefactorize(const MatrixType& A0,
                                                            const std::vector<int>& subCols,
                                                            const std::vector<int>& rowMapA0,
                                                            int* flag_,
                                                            int* iws_,
                                                            double* ws_,
                                                            int wslen_)
    {

        if(!flag_) flag_ = flag.data();
        if(!iws_) iws_ = iwsN;
        if(!ws_)
        {
            ws_ = ws;
            wslen_ = wslen;
        }

        const int NS = L.NS;

        std::fill_n(flag_, NS, true);
        assert(std::all_of(ws_, ws_ + wslen_, [](const double d){return d == .0;}));

        std::vector<int> columnFlag(NS, -1);

        int top = topDirtyNodes;


        for(; top < NS; ++top)
        {
            int i = dirtyNodes[top];
            int ss = L.supernodeSizes[i];

            // use 'flags' to save a mapping of row indizes to the start of values in supernode 'i'
            int vidx = 0;

            for(int j = L.cols[i]; j < L.cols[i+1]; ++j, ++vidx)
            {
                flag_[L.rows[j]] = vidx;
            }

            int nrows = L.cols[i+1] - L.cols[i];

            int k0 = L.rows[L.cols[i]];
            int k1 = k0 + ss;

            // clear supernode
            int offset = L.snodeValueStart[i];

            // copy A[k0:k1, 0:k1] into L
            for(int k = k0; k < k1; ++k, offset += nrows)
            {
                int cA0 = subCols[k]; // copy column 'cA0' in 'A0' to factor

                for(int j = A0.diag[cA0]; j < A0.col[cA0+1]; ++j)
                {
                    const int idr = rowMapA0[A0.row[j]];

                    if(idr != -1)
                    {
                        L.vals[flag_[idr] + offset] = A0.vals[j];
                    }

                }
            }

            // compute L(k0:k1, 1:k1-1) * L(k0:k1, 1:k1-1)' and substract from A (already copied into L)
            for(int k = k0; k < k1; ++k)
            {
                for(int j = startColsInRow[k]; j < startColsInRow[k+1]; ++j)
                {
                    const int c = colsInRow[j];

                    if(c < i && columnFlag[c] != i)
                    {
                        columnFlag[c] = i;

                        int sr = colsInRowColIndex[j];
                        int ssc = L.supernodeSizes[c];

                        int nrows = (L.cols[c+1] - sr);
                        double* vstart = &L.vals[L.snodeValueStart[c] + (sr - L.cols[c])];

                        // ndrows = #rows in [k0:k1]
                        int ndrows = 0;
                        for(int k = sr; k < L.cols[c+1] && L.rows[k] < k1; ++k, ++ndrows);

                        int m1 = nrows - ndrows;
                        int stride = L.cols[c+1] - L.cols[c];

                        dsyrk_(&cL, &cN,
                               &ndrows, &ssc,
                               &one,
                               vstart, &stride,
                               &zero,
                               ws_, &nrows);

                        dgemm_(&cN, &cT,
                               &m1, &ndrows, &ssc,
                               &one,
                               vstart + ndrows, &stride,
                               vstart, &stride,
                               &zero,
                               ws_ + ndrows, &nrows);

                        int k2 = 0;
                        double* vptr = ws_;
                        double* baseDest = &L.vals[L.snodeValueStart[i]];

                        for(int k = k0; k < k1; ++k)
                        {
                            if(L.rows[sr + k2] == k) // is column 'k' present in ws
                            {
                                for(int l = sr; l < L.cols[c+1]; ++l)
                                {
                                    baseDest[flag_[L.rows[l]]] -= *vptr;
                                    *vptr++ = .0;
                                }

                                ++k2;
                            }

                            baseDest += L.cols[i + 1] - L.cols[i];
                        }
                    }
                }
            }

            // dense cholesky factorization of diagonal block
            dpotrf_(&cL, &ss, &L.vals[L.snodeValueStart[i]], &nrows, &info);
            if(info) std::cout << "dpotrf error, matrix is not numerical postive definite" << std::endl;

            int tailRows = nrows - ss;

            // solve triangular system to compute tail of supernode 'i'
            dtrsm_(&cR, &cL, &cT, &cN,
                   &tailRows, &ss, &one,
                   &L.vals[L.snodeValueStart[i]], &nrows,
                   &L.vals[L.snodeValueStart[i] + ss], &nrows);

            k0 = k1;
        }

        assert(std::all_of(ws_, ws_ + wslen_, [](const double d){return d == .0;}));
    }

    struct CopyChunk
    {
        double* start;
        double* beyond;
        double* dest;

        inline void
        process() const
        {
            std::copy(start, beyond, dest);
        }
    };

    struct SetZeroChunk
    {
        double* start;
        double* beyond;

        inline void
        process() const
        {
            std::fill(start, beyond, 0.);
        }
    };

    template<int NumThreads, class Chunk>
    void parallelCopy(const Chunk* start, const Chunk* beyond, const int totalLen)
    {

        // split the chunks into NumThreads segments to be
        // processed in parallel.

        auto copyData = [](const Chunk* st, const Chunk* bey)
        {
            for(auto it = st; it != bey; ++it)
            {
                it->process();
            }
        };

        const int len = (int)std::distance(start, beyond);

        if(len <  NumThreads || NumThreads == 1) // proceed with one thread only
        {
            copyData(start, beyond);
            return ;
        }

        auto segmentPointers = new const Chunk*[NumThreads + 1];
        segmentPointers[0] = start;
        segmentPointers[NumThreads] = beyond;

        double sum = 0;
        int cut = totalLen / (double)NumThreads;
        int i = 1;


        for(auto it = start; it != beyond; ++it)
        {
            sum += std::distance(it->start, it->beyond);

            if(sum > cut)
            {
                segmentPointers[i] = it;
                cut += totalLen / (double)NumThreads;
                ++i;

                if(i == NumThreads) break;
            }
        }

        for(; i < NumThreads; ++i)
            segmentPointers[i] = beyond;

        assert(i == NumThreads);

        std::array<std::thread, NumThreads> threads;

        for(int i = 0; i < NumThreads; ++i)
            threads[i] = std::thread(copyData, segmentPointers[i], segmentPointers[i+1]);

        for(int i = 0; i < NumThreads; ++i)
            threads[i].join();

        delete[] segmentPointers;
    }

    template<class MatrixType>
    void
    SupernodalCholesky<MatrixType>::copySupernodes(const std::vector<int>& scols,
                                                   const std::vector<int>& rowMap,
                                                   const int* skipStart, const int* skipEnd,
                                                   SupernodalCholesky<MatrixType>& f2)
    {

        // we need the skip list sorted!

        const int nSkip = (int)std::distance(skipStart, skipEnd);
        int* skipSorted = new int[nSkip];
        std::partial_sort_copy(skipStart, skipEnd, skipSorted, skipSorted + nSkip);


        int currSn = -1;
        int k0 = -1;
        int nrows = -1;
        int sn2 = -1;

        double* vptr = f2.L.vals;
        int* rptr = f2.L.rows;

        SetZeroChunk* zchunk = new SetZeroChunk[f2.L.numcols];
        SetZeroChunk* zcptr = zchunk;
        int zeroCount = 0;
        int valueCount = 0;

        CopyChunk* vchunk = new CopyChunk[f2.L.NNZ];
        CopyChunk* vcptr = vchunk;

        CopyChunk *snstart = nullptr, *snend = nullptr; // use values from the first column of each superrow
        double *vstart = nullptr;


        auto itSkip = skipSorted;
        bool skipping = false;
        int rowsInNode = 0;

        for(int j = 0; j < scols.size(); ++j)
        {
            const int i = scols[j];
            int sn = L.colMap[i];

            if(currSn == sn)
            {
                ++f2.L.supernodeSizes[sn2];

                if(!skipping)
                {
                    // index of column 'i' in supernode 'sn'
                    const int ki = i - k0;
                    double* v = L.vals + L.snodeValueStart[sn] + ki * nrows;
                    int vOffset = (int)std::distance(vstart, v);

                    for(auto it = snstart; it != snend; ++it, ++vcptr)
                    {

                        vcptr->start = it->start + vOffset;
                        vcptr->beyond = it->beyond + vOffset;
                        vcptr->dest = vptr;

                        valueCount += std::distance(vcptr->start, vcptr->beyond);
                        vptr += std::distance(vcptr->start, vcptr->beyond);
                    }
                } else
                {
                    vptr += rowsInNode;
                }

            } else
            {

                ++sn2;

                if(skipping)
                {
                    zcptr->beyond = vptr;
                    zeroCount += std::distance(zcptr->start, zcptr->beyond);
                    ++zcptr;
                }

                if(itSkip != skipSorted + nSkip && sn2 == *itSkip)
                {
                    ++itSkip;

                    skipping = true;

                } else skipping = false;


                currSn = sn;
                k0 = L.rows[L.cols[sn]];

                nrows = L.cols[sn+1] - L.cols[sn];

                f2.L.snodeValueStart[sn2] = (int)std::distance(f2.L.vals, vptr);
                f2.L.supernodeSizes[sn2] = 1;

                // index of column 'i' in supernode 'sn'
                const int ki = i - k0;
                double* v = L.vals + L.snodeValueStart[sn] + ki * nrows;

                // copy row information and data
                const int len = L.cols[sn+1] - L.cols[sn];
                int* itr = &L.rows[L.cols[sn]];

                snstart = vcptr;
                vstart = v;

                if(skipping)
                {
                    rowsInNode = 0;

                    for(int j = 0; j < len; ++j)
                    {
                        const int idx = rowMap[*itr++];

                        if(idx != -1)
                        {
                            *rptr++ = idx;
                            ++rowsInNode;
                        }
                    }

                    zcptr->start = vptr;
                    vptr += rowsInNode;

                } else
                {

                    bool f = false;
                    int jstart = -1;

                    for(int j = 0; j < len; ++j)
                    {
                        const int idx = rowMap[*itr++];

                        if(idx != -1)
                        {
                            if(!f)
                            {
                                f = true;

                                jstart = j;
                                vcptr->start = v  + j;
                                vcptr->dest = vptr;

                            }

                            *rptr++ = idx;


                        } else
                        {
                            if(f)
                            {
                                f = false;
                                vcptr->beyond = v  + j;
                                ++vcptr;

                                valueCount += j - jstart;
                                vptr += j - jstart;
                            }
                        }
                    }

                    if(f)
                    {
                        vcptr->beyond = v + len;
                        ++vcptr;
                        valueCount += len - jstart;


                        vptr += len - jstart;
                    }

                    snend = vcptr;
                }


                assert(f2.L.NS + 1 > sn2 + 1);
                f2.L.cols[sn2+1] = (int)std::distance(f2.L.rows, rptr);
            }
        }

        if(skipping)
        {
            zcptr->beyond = vptr;
            zeroCount += std::distance(zcptr->start, zcptr->beyond);
            ++zcptr;
        }

        parallelCopy<NUMTHREADS>(vchunk, vcptr, valueCount);
        parallelCopy<NUMTHREADS>(zchunk, zcptr, zeroCount);

        delete[] skipSorted;
        delete[] zchunk;
        delete[] vchunk;
    }



    template<class MatrixType>
    void
    SupernodalCholesky<MatrixType>::subfactor(const std::vector<int>& scols,
                                              const std::vector<int>& rowMap,
                                              const std::vector<int>& updateSeeds,
                                              SupernodalCholesky<MatrixType>& f2,
                                              int* stats)

    {
        assert(is_sorted(scols.begin(), scols.end()));
        assert(!scols.empty());

        if(scols.empty()) return;

        f2.N = (int)scols.size();
        const int N2 = (int)scols.size();

        f2.L.colMap = new int[scols.size()];

        // space requirements, set structural data structures
        int NS2 = -1;
        int csn = -1;
        int sncnt = 0;

        // count number of supernodes in f2.L
        // and set f2.colMap
        for(int i = 0; i < scols.size(); ++i)
        {
            const int sn = L.colMap[scols[i]];

            if(sn == csn)
            {
                ++sncnt;
            } else
            {
                ++NS2;
                csn = sn;
            }

            f2.L.colMap[i] = NS2;
        }

        ++NS2;

        // fill 'f2.cols', 'f2.supernodeSizes' and 'f2.snodeValueStart'
        f2.L.cols = new int[NS2 + 1];
        f2.L.cols[0] = 0;

        f2.L.supernodeSizes = new int[NS2];
        f2.L.snodeValueStart = new int[NS2];
        f2.L.snodeValueStart[0] = 0;


        f2.startColsInRow = new int[N2 + 1];
        std::fill_n(f2.startColsInRow, N2 + 1, 0);

        csn = -2;
        int currSn = 0;
        int NR2 = 0;
        int rowCount = 0;

        for(int i = 0; i < scols.size(); ++i)
        {
            int sn = L.colMap[scols[i]];

            if(sn != csn)
            {
                if(currSn)
                {
                    f2.L.supernodeSizes[currSn-1] = sncnt;
                    f2.L.snodeValueStart[currSn] = f2.L.snodeValueStart[currSn-1] + rowCount * sncnt;
                    f2.L.cols[currSn] = NR2;
                }


                rowCount = 0;
                sncnt = 1;
                csn = sn;

                // count entries
                for(int j = L.cols[sn]; j < L.cols[sn+1]; ++j)
                {
                    const int idx = rowMap[L.rows[j]];

                    if(idx != -1)
                    {
                        ++f2.startColsInRow[idx];
                        ++rowCount;
                    }
                }

                NR2 += rowCount;
                ++currSn;
            } else
            {
                ++sncnt;
            }
        }

        // accumulate row counts
        int sum = 0;
        for(int i = 0; i <= N2; ++i)
        {
            int tmp = f2.startColsInRow[i];
            f2.startColsInRow[i] = sum;
            sum += tmp;
        }


        f2.L.numcols = f2.L.numrows = (int)scols.size();
        f2.L.NS = NS2;
        f2.L.cols[NS2] = NR2;
        f2.L.supernodeSizes[NS2-1] = sncnt;

        int NNZ2 = f2.L.snodeValueStart[NS2 - 1] + rowCount * sncnt;

        f2.L.vals = new double[NNZ2];
        f2.L.NNZ = NNZ2;

        f2.L.rows = new int[NR2];
        f2.L.NR = NR2;

        int* rptr = f2.L.rows;

        // copy row and row-column data (needs to be done now to compute setree)
        f2.colsInRow = new int[NR2];
        f2.colsInRowColIndex = new int[NR2];

        csn = -1;
        currSn = 0;
        int cnt = 0;


        int* cir = f2.colsInRow;
        int* circi = f2.colsInRowColIndex;
        int* scir = f2.startColsInRow;


        for(int i : scols)
        {
            const int sn = L.colMap[i];

            if(sn != csn)
            {

                for(int j = L.cols[sn]; j < L.cols[sn+1]; ++j)
                {
                    const int idx = rowMap[L.rows[j]];

                    if(idx != -1)
                    {
                        *rptr++ = idx;

                        const int id2 = scir[idx]++;

                        cir[id2] = currSn;
                        circi[id2] = cnt++;
                    }
                }

                csn = sn;
                ++currSn;
            }
        }

        // reset colsInRow
        for(int i = N2; i > 0; --i)
            f2.startColsInRow[i] = f2.startColsInRow[i-1];

        f2.startColsInRow[0] = 0;


        // build supernodal elimination tree
        f2.setree = new int[NS2];

        for(int i = 0; i < NS2; ++i)
        {
            int id  = f2.L.cols[i] + f2.L.supernodeSizes[i];

            if(id < f2.L.cols[i+1])
            {
                f2.setree[i] = f2.L.colMap[f2.L.rows[id]];
            } else f2.setree[i] = -1;
        }


        // find dependant nodes
        f2.dirtyNodes = new int[NS2];
        f2.topDirtyNodes = f2.getDependantSupernodes(updateSeeds, f2.dirtyNodes);

        // copy value data in parallel
        copySupernodes(scols, rowMap, f2.dirtyNodes + f2.topDirtyNodes, f2.dirtyNodes + f2.L.NS, f2);
    }


    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::update(SparseMatrix<double>& W)
    {
        assert(W.ncols == 1);
        assert(std::all_of(ws, ws + wslen, [](const double d){return d == 0.;}));

        // scatter single column in W into ws
        for(int i = W.col[0]; i < W.col[1]; ++i)
        {
            ws[W.row[i]] = W.vals[i];
        }

        double alpha, beta = 1, delta, gamma, w1, beta2 = 1 ;
        int j = L.colMap[W.row[0]];

        // update all supernodes along the path starting at 'j'
        for( ; j != -1; j = setree[j])
        {
            // update all columns in supernode
            int ss = L.supernodeSizes[j];
            int vp = L.snodeValueStart[j];
            int row0 = L.rows[L.cols[j]];

            for(int k = 0; k < ss; ++k)
            {
                vp += k; // supernodes store dense triangular blocks, skip first k entries

                alpha = ws[row0 + k] / L.vals[vp];
                beta2 = beta * beta + alpha * alpha;

                beta2 = sqrt(beta2);
                delta = beta / beta2;
                gamma = alpha / (beta2 * beta);

                L.vals[vp] = delta * L.vals[vp] + gamma * ws[row0 + k];
                beta = beta2;

                ws[row0 + k] = .0;

                ++vp;
                for(int i = L.cols[j] + k + 1; i < L.cols[j+1]; ++i, ++vp)
                {
                    w1 = ws[L.rows[i]];
                    ws[L.rows[i]] = w1 - alpha * L.vals[vp];
                    L.vals[vp] = delta * L.vals[vp] + gamma * w1;
                }
            }
        }
    }


    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::solve(Matrix<T>& m)
    {
        if(iperm.empty())
        {
            assert(N = m.nrows);
            solveL(m);
            solveLT(m);
            return;
        }

        Matrix<T> tmp(N, m.ncols);

        for(int j = 0; j < m.ncols; ++j)
        {
            for(int i = 0; i < N; ++i)
            {
                tmp(i, j) = m(iperm[i], j) ;
            }
        }

        // Timer t;

        solveL(tmp);
        solveLT(tmp);

        // t.printTime("solve inner");

        for(int j = 0; j < m.ncols; ++j)
        {
            for(int i = 0; i < N; ++i)
            {
                m(iperm[i], j) = tmp(i, j);
            }
        }
    }


    template<class MatrixType>
    template<int Cols>
    void SupernodalCholesky<MatrixType>::solveL_RowMajor(T* md)
    {
        const int NS = L.NS;

        std::array<T, Cols> buff;
        int c = 0;

        for(int i = 0; i < NS; ++i)
        {
            T* vals = L.vals + L.snodeValueStart[i];

            const int sns = L.supernodeSizes[i];

            for(int k = 0; k < sns; ++k, ++c)
            {
                vals += k; //offset since head of supernode is stored with explicit zeros

                for(int m = 0; m < Cols; ++m)  // divide complete row by diagonal
                    buff[m] = (md[Cols * iperm[c] + m] /= *vals);

                ++vals;

                for(int j = L.cols[i] + k + 1; j < L.cols[i+1]; ++j)
                {
                    const int off = Cols * iperm[L.rows[j]];
                    const auto v = *vals++;

                    for(int m = 0; m < Cols; ++m)
                    {
                        md[off + m] -= v * buff[m] ;
                    }
                }
            }
        }
    }

    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::solve3_RowMajor(T* md)
    {
        solveL_RowMajor<3>(md);
        solveLT_RowMajor<3>(md);
    }

    template<class MatrixType>
    template<int Cols>
    void SupernodalCholesky<MatrixType>::solveLT_RowMajor(T* md)
    {
        const int NS = L.NS;

        std::array<T, Cols> buff;
        int c = L.numcols - 1;

        for(int i = NS - 1; i >= 0; --i)
        {
            const int sns = L.supernodeSizes[i];

            for(int k = sns-1; k >= 0; --k, --c)
            {
                T* vals = L.vals + L.snodeValueStart[i] + k * (L.cols[i + 1] - L.cols[i]) + k;
                const auto diag = *vals++;

                for(int m = 0; m < Cols; ++m)  // divide complete row by diagonal
                    buff[m] = md[Cols * iperm[c] + m] ;

                for(int j = L.cols[i] + k + 1; j < L.cols[i+1]; ++j)
                {
                    const auto v = *vals++;
                    const int off = Cols * iperm[L.rows[j]];

                    for(int m = 0; m < Cols; ++m)
                    {
                        buff[m] -= v * md[off + m] ;
                    }
                }

                for(int m = 0; m < Cols; ++m)  // divide complete row by diagonal
                    md[Cols * iperm[c] + m] = buff[m] / diag;
            }
        }
    }

    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::solveL(Matrix<T>& m)
    {

        const int NS = L.NS;//L.supernodeSizes.size();
        double* md = m.data; //m.dataVector().data();
        int NC = m.cols();
        int NR = m.rows();
        int k0 = 0;

        assert(ws && std::all_of(ws, ws + wslen, [](const double d){return d == .0;}));

        for(int i = 0; i < NS; ++i)
        {
            int ss = L.supernodeSizes[i];
            int k1 = k0 + ss;
            int rowsi = L.cols[i+1] - L.cols[i];
            int rows2 = rowsi - ss;
            int vstart = L.snodeValueStart[i];

            dtrsm_(&cL, &cL, &cN, &cN, &ss, &NC, &one, &L.vals[vstart], &rowsi, &md[k0], &NR);

            if(rows2 > 0)
            {
                dgemm_(&cN, &cN, &rows2, &NC, &ss, &one, &L.vals[vstart + ss], &rowsi, &md[k0], &NR, &zero, ws, &rows2);

                int id = L.cols[i] + ss;

                for(int j = 0; j < rows2; ++j, ++id)
                {
                    for(int k = 0; k < NC; ++k)
                    {
                        md[L.rows[id] + k * NR] -= ws[j + k * rows2];
                    }
                }
            }

            k0 = k1;
        }
    }

    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::solveLT(Matrix<T>& m)
    {
        const int NS = L.NS;//L.supernodeSizes.size();
        double* md = m.data; //m.dataVector().data();
        int NC = m.cols();
        int NR = m.rows();
        int k1 = L.numcols;

        for(int i = NS - 1; i >= 0; --i)
        {
            int ss = L.supernodeSizes[i];
            int k0 = k1 - ss;
            int rowsi = L.cols[i+1] - L.cols[i];
            int rows2 = rowsi - ss;
            int vstart = L.snodeValueStart[i];

            if(rows2 > 0)
            {
                int id = 0;
                for(int j = L.cols[i] + ss; j < L.cols[i+1]; ++j, ++id)
                {
                    for(int k = 0; k < NC; ++k)
                    {
                        ws[id + k * rows2] = md[L.rows[j] + k * NR];
                    }
                }

                dgemm_(&cT, &cN, &ss, &NC, &rows2, &minus_one , &L.vals[vstart + ss], &rowsi, ws, &rows2, &one, &md[k0], &NR);
            }

            dtrsm_(&cL, &cL, &cT, &cN, &ss, &NC, &one, &L.vals[vstart], &rowsi, &md[k0], &NR);


            k1 = k0;
        }
    }

    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::numeric(const MatrixType& A)
    {

        const int NS = L.NS;
        int k0 = 0;

        std::fill_n(L.vals, L.NNZ, 0.0);
        std::fill_n(ws, wslen, 0.0);

        std::vector<int> columnFlag(NS, -1);

        for(int i = 0; i < NS; ++i)
        {
            int ss = L.supernodeSizes[i];

            // use 'flags' to save a mapping of row indizes to the start of values in supernode 'i'
            int vidx = 0;

            for(int j = L.cols[i]; j < L.cols[i+1]; ++j, ++vidx)
            {
                flag[L.rows[j]] = vidx;
            }

            int nrows = L.cols[i+1] - L.cols[i];

            int k1 = k0 + ss;

            // copy A[k0:k1, 0:k1] into L
            int offset = L.snodeValueStart[i];

            for(int k = k0; k < k1; ++k, offset += nrows)
            {
                for(int j = A.diag[k]; j < A.col[k+1]; ++j)
                {
                    L.vals[flag[A.row[j]] + offset] = A.vals[j];
                }
            }

            // compute L(k0:k1, 1:k1-1) * L(k0:k1, 1:k1-1)' and substract from A (already copied into L)
            for(int k = k0; k < k1; ++k)
            {
                for(int j = startColsInRow[k]; j < startColsInRow[k+1]; ++j)
                {
                    const int c = colsInRow[j];

                    if(c < i && columnFlag[c] != i)
                    {
                        columnFlag[c] = i;

                        int sr = colsInRowColIndex[j];
                        int ssc = L.supernodeSizes[c];

                        int nrows = (L.cols[c+1] - sr);
                        double* vstart = &L.vals[L.snodeValueStart[c] + (sr - L.cols[c])];

                        // ndrows = #rows in [k0:k1]
                        int ndrows = 0;
                        for(int k = sr; k < L.cols[c+1] && L.rows[k] < k1; ++k, ++ndrows);

                        int m1 = nrows - ndrows;
                        int stride = L.cols[c+1] - L.cols[c];


                        dsyrk_(&cL, &cN,
                               &ndrows, &ssc,
                               &one,
                               vstart, &stride,
                               &zero,
                               ws, &nrows);

                        dgemm_(&cN, &cT,
                               &m1, &ndrows, &ssc,
                               &one,
                               vstart + ndrows, &stride,
                               vstart, &stride,
                               &zero,
                               ws + ndrows, &nrows);

                        int k2 = 0;
                        double* vptr = ws;
                        double* baseDest = &L.vals[L.snodeValueStart[i]];

                        for(int k = k0; k < k1; ++k)
                        {
                            if(L.rows[sr + k2] == k) // is column 'k' present in ws?
                            {
                                for(int l = sr; l < L.cols[c+1]; ++l)
                                {
                                    baseDest[flag[L.rows[l]]] -= *vptr;
                                    *vptr++ = .0;
                                }

                                ++k2;
                            }

                            baseDest += L.cols[i + 1] - L.cols[i];
                        }
                    }
                }
            }


            // dense cholesky factorization of diagonal block
            dpotrf_(&cL, &ss, &L.vals[L.snodeValueStart[i]], &nrows, &info);

            if(info)
            {
                std::cout << "dpotrf error, matrix is not numerical postive definite" << std::endl;
            }

            int tailRows = nrows - ss;

            // solve triangular system to compute tail of supernode 'i'
            dtrsm_(&cR, &cL, &cT, &cN,
                   &tailRows, &ss, &one,
                   &L.vals[L.snodeValueStart[i]], &nrows,
                   &L.vals[L.snodeValueStart[i] + ss], &nrows);

            k0 = k1;
        }
    }


    template<class MatrixType>
    void SupernodalCholesky<MatrixType>::symbolic(const MatrixType& A)
    {

        // compute etree of for A
        int* colCount = new int[N];
        etree = new int[N];
        iwsN = new int[N];
        iwsN2 = new int[N];

        for(int i = 0; i < N; ++i)
        {
            etree[i] = -1;
            iwsN[i] = -1;

            const int beyond = A.diag[i];
            for(int p = A.col[i]; p != beyond; ++p)
            {
                int k = A.row[p];
                int a;

                for(;;)
                {
                    a = iwsN[k];

                    if(a == i) break;

                    iwsN[k] = i;

                    if(a == -1)
                    {
                        etree[k] = i;
                        break;
                    }

                    k = a;
                }
            }
        }

        postOrdering(etree, N, iwsN);
        computeColCounts(A, etree, iwsN, colCount);

        // form supernodes
        // count number of childs in 'iws2'

        std::fill(iwsN2, iwsN2 + N, 0);

        for(int i = 0; i < N; ++i)
        {
            if(etree[i] != -1)
                ++iwsN2[etree[i]];
        }

        int NS = 0;
        iwsN[0] = 1;

        L.colMap = new int[N];
        L.colMap[0] = 0;

        for(int i = 1; i < N; ++i)
        {
            if(etree[i-1] != i || (colCount[i-1] != colCount[i] + 1) || iwsN2[i] > 1)
            {
                ++NS;
                iwsN[NS] = 0;
            }

            L.colMap[i] = NS;
            ++iwsN[NS];
        }

        ++NS;

        // compute size of workspace as max values of a supernode
        int k0 = 0;
        wslen = 0;

        for(int i = 0; i < NS; ++i)
        {
            if(wslen < colCount[k0] * iwsN[i])
                wslen = colCount[k0] * iwsN[i];

            k0 += iwsN[i];
        }

        ws = new double[wslen];

        L.supernodeSizes = new int[NS];
        std::memcpy((void*)L.supernodeSizes, (void*)iwsN, NS * sizeof(int));

        L.numcols = N;

        L.snodeValueStart = new int[NS];
        L.cols = new int[NS + 1];
        L.NS = NS;

        L.cols[0] = 0;

        k0 = 0;
        int vcnt = 0;

        for(int i = 0; i < NS; ++i)
        {
            L.snodeValueStart[i] = vcnt;
            vcnt += colCount[k0] * L.supernodeSizes[i];
            L.cols[i+1] = L.cols[i] + colCount[k0];
            k0 += L.supernodeSizes[i];
        }

        delete[] colCount;

        if(L.rows || L.vals) std::cout << "should not be allocated!" << std::endl;

        L.NR = L.cols[NS];
        L.rows = new int[L.NR];

        L.NNZ = vcnt;
        L.vals = new double[L.NNZ];


        // compute setree by contracting supernodes in the etree
        setree = new int[NS];
        k0 = -1;

        for(int i = 0; i < NS; ++i)
        {
            k0 += L.supernodeSizes[i];

            int parent = etree[k0];
            setree[i] = parent == -1 ? -1 : L.colMap[parent];
        }

        // compute column/row indizes
        std::fill_n(flag.data(), NS, 0);
        int* rptr = L.rows;

        int* colsInRowPtr = colsInRow = new int[L.NR];
        int* colsInRowColIndexPtr = colsInRowColIndex = new int[L.NR];
        startColsInRow = new int[L.numcols + 1];
        startColsInRow[0] = 0;

        k0 = 0;
        int ncr = 0;

        for(int j = 0; j < NS; ++j)
        {
            int k1 = k0 + L.supernodeSizes[j];

            for (int k = k0; k < k1 ; ++k)
            {
                rptr[L.cols[j]++] = k;
            }

            for(int k = k0; k < k1; ++k)
            {
                flag[j] = k;

                const int beyond = A.diag[k] + 1;
                for(int p = A.col[k]; p != beyond; ++p)
                {
                    for(int i = L.colMap[A.row[p]]; flag[i] < k; i = setree[i])
                    {
                        flag[i] = k;
                        ++ncr;

                        *colsInRowPtr++ = i;
                        *colsInRowColIndexPtr++ = L.cols[i];

                        rptr[L.cols[i]++] = k;
                    }
                }

                startColsInRow[k+1] = ncr;
            }

            k0 = k1;
        }


        // reset column pointers
        for(int i = NS; i > 0; --i)
            L.cols[i] = L.cols[i-1];

        L.cols[0] = 0;
    }

} /* namespace CholUp */
