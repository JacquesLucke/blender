#include "EliminationTreeMethods.h"
#include "SparseMatrix.h"

namespace CholUp {

int ereach(const SparseMatrix<double>& A, int NS, int k, int k0, const int* parent, const int* colMap, int* s, int* w)
{
    int i, len;
    int top = NS;
    int sk = colMap[k];

    w[sk] = k;

    for(int p = A.col[k]; p < A.diag[k]; ++p)
    {
        i = colMap[A.row[p]];

        for(len = 0; w[i] < k0; i = parent[i])
        {
            s[len++] = i;
            w[i] = k;
        }

        while(len > 0) s[--top] = s[--len];
    }

    return top;
}

void firstdesc (int n, int *parent, int *post, int *first, int *level)
{
    int len, i, k, r, s ;
    for (i = 0 ; i < n ; i++) first [i] = -1 ;
    for (k = 0 ; k < n ; k++)
    {
        i = post [k] ; /* node i of etree is kth postordered node */ len =0; /* traverse from i towards the root */
        for (r = i ; r != -1 && first [r] == -1 ; r = parent [r], len++)
            first [r] = k ;
        len += (r == -1) ? (-1) : level [r] ;   /* root node or end of path */
        for (s = i ; s != r ; s = parent [s]) level [s] = len-- ;
    }
}

int tdfs(int j, int k, int *head, const int *next, int *post, int *stack)
{
    int i, p, top = 0;
    if (!head || !next || !post || !stack) return -1;

    stack[0] = j;

    while(top >= 0)
    {
        p = stack[top];
        i = head[p];
        if(i == -1)
        {
            --top;
            post[k++] = p;
        } else
        {
            head[p] = next[i];
            stack[++top] = i;
        }
    }

    return k;
}

void postOrdering(const int* parent, const int n, int* post)
{
    int k = 0;
    int* w = new int[3 * n];

    int* head = w;
    int* next = w + n;
    int* stack = w + 2 * n;

    for(int j = 0; j < n; ++j) head[j] = -1;

    for(int j = n-1; j >= 0; --j)
    {
        if(parent[j] == -1) continue;

        next[j] = head[parent[j]];
        head[parent[j]] = j;
    }

    for(int j = 0; j < n; ++j)
    {
        if(parent[j] != -1) continue;
        k = tdfs(j, k, head, next, post, stack);
    }

    delete[] w;
}

int leaf (int i, int j, const int *first, int *maxfirst, int *prevleaf,
          int *ancestor, int *jleaf)
{
    int q, s, sparent, jprev ;
    if (!first || !maxfirst || !prevleaf || !ancestor || !jleaf) return (-1) ;
    *jleaf = 0 ;
    if (i <= j || first [j] <= maxfirst [i]) return (-1) ;  /* j not a leaf */
    maxfirst [i] = first [j] ;      /* update max first[j] seen so far */
    jprev = prevleaf [i] ;          /* jprev = previous leaf of ith subtree */
    prevleaf [i] = j ;
    *jleaf = (jprev == -1) ? 1: 2 ; /* j is first or subsequent leaf */
    if (*jleaf == 1) return (i) ;   /* if 1st leaf, q = root of ith subtree */
    for (q = jprev ; q != ancestor [q] ; q = ancestor [q]) ;
    for (s = jprev ; s != q ; s = sparent)
    {
        sparent = ancestor [s] ;    /* path compression */
        ancestor [s] = q ;
    }
    return (q) ;                    /* q = least common ancester (jprev,j) */
}

} /* namespace CholUp */