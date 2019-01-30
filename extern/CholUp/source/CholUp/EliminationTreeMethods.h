#pragma once

#include "SparseMatrix.h"

namespace CholUp {

int tdfs(int j, int k, int *head, const int *next, int *post, int *stack);

void postOrdering(const int* parent, const int n, int* post);

int leaf (int i, int j, const int *first, int *maxfirst, int *prevleaf,
          int *ancestor, int *jleaf);

void firstdesc (int n, int *parent, int *post, int *first, int *level);

int ereach(const SparseMatrix<double>& A, int NS, int k, int k0, const int* parent, const int* colMap, int* s, int* w);

template<class MatrixType>
void computeColCounts(const MatrixType& A, int* parent, int* post, int* colcount /* out*/)
{
    int i, j, k, n, J, s, p, q, jleaf, *maxfirst, *prevleaf,
    *ancestor, *w, *first, *delta ;
    n = A.ncols;

    s = 4*n;

    delta = colcount;
    w = new int[s];                   /* get workspace */

    ancestor = w ; maxfirst = w+n ; prevleaf = w+2*n ; first = w+3*n ;
    for (k = 0 ; k < s ; k++) w [k] = -1 ;      /* clear workspace w [0..s-1] */

    for (k = 0 ; k < n ; k++)                   /* find first [j] */
    {
        j = post [k] ;
        delta [j] = (first [j] == -1) ? 1 : 0 ;  /* delta[j]=1 if j is a leaf */
        for ( ; j != -1 && first [j] == -1 ; j = parent [j]) first [j] = k ;
    }

    const int *ATp = A.col;
    const int *ATi = A.row;

    for (i = 0 ; i < n ; i++) ancestor [i] = i ; /* each node in its own set */

    for (k = 0 ; k < n ; k++)
    {
        j = post [k] ;          /* j is the kth node in postordered etree */
        if (parent [j] != -1) delta [parent [j]]-- ;    /* j is not a root */
        for (J = j ; J != -1 ; J = -1)   /* J=j for LL'=A case */
        {
            for (p = A.diag[J] + 1 ; p < ATp [J+1] ; p++)
            {
                i = ATi [p] ;
                q = leaf (i, j, first, maxfirst, prevleaf, ancestor, &jleaf);
                if (jleaf >= 1) delta [j]++ ;   /* A(i,j) is in skeleton */
                if (jleaf == 2) delta [q]-- ;   /* account for overlap in q */
            }
        }

        if (parent [j] != -1) ancestor [j] = parent [j] ;
    }
    for (j = 0 ; j < n ; j++)           /* sum up delta's of each child */
    {
        if (parent [j] != -1) colcount [parent [j]] += colcount [j] ;
    }

    delete[] w;
}

template<class MatrixType>
void rowCounts(const MatrixType& A, int* parent, int* post, int* rowcount)
{
    int n = A.col.size() - 1;
    const int* Ap = A.col.data();
    const int* Ai = A.row.data();      /* get A */

    int* w = new int[5 * n]; /* get workspace */
    auto ancestor = w ;
    auto maxfirst = w+n ;
    auto prevleaf = w+2*n ;
    auto first = w+3*n ;
    auto level = w+4*n ;

    firstdesc(n, parent, post, first, level) ;

    for(int i = 0 ; i < n ; i++)
    {
        rowcount [i] = 1;  /* count the diagonal of L */
        prevleaf [i] = -1; /* no previous leaf of the ith row subtree */
        maxfirst [i] = -1; /* max first[j] for node j in ith subtree */
        ancestor [i] = i;  /* every node is in its own set, by itself */
    }

    int jleaf;

    for(int k = 0 ; k < n ; k++)
    {
        int j = post [k];  /* j is the kth node in the postordered etree */
        for (int p = Ap [j] ; p < Ap [j+1] ; p++)
        {
            int i = Ai [p] ;
            int q = leaf (i, j, first, maxfirst, prevleaf, ancestor, &jleaf) ;
            if (jleaf) rowcount [i] += (level [j] - level [q]) ;
        }

        if (parent [j] != -1) ancestor [j] = parent [j] ;

    }

    delete[] w;
}

} /* namespace CholUp */