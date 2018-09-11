/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __EIGEN3_SPARSE_MATRIX_C_API_H__
#define __EIGEN3_SPARSE_MATRIX_C_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SparseMatrixF SparseMatrixF;
typedef struct MatrixFEntries MatrixFEntries;
typedef struct SparseLDLTDecompositionF SparseLDLTDecompositionF;
typedef struct SparseLeastSquaresSystemF SparseLeastSquaresSystemF;

MatrixFEntries *EIG_MatrixFEntries_New();
void EIG_MatrixFEntries_Delete(MatrixFEntries *entries);
void EIG_MatrixFEntries_Add(MatrixFEntries *entries, int row, int column, float value);

SparseMatrixF *EIG_SparseMatrixF_FromEntries(int rows, int columns, MatrixFEntries *entries);
void EIG_SparseMatrixF_Delete(SparseMatrixF *matrix);

SparseLDLTDecompositionF *EIG_SparseMatrixF_LDLTDecomposition(SparseMatrixF *matrix);
SparseMatrixF *EIG_SparseMatrixF_LLTDecomposition(SparseMatrixF *matrix);
void EIG_SparseLDLTDecompositionF_Delete(SparseLDLTDecompositionF *decomposition);

void EIG_SparseLLTSolveF(SparseMatrixF *L, float *b, float *x);
void EIG_SparseLDLTSolveF(SparseLDLTDecompositionF *decomposition, float *b, float *x);
void EIG_SparseLowerTriangularSolveF(SparseMatrixF *L, float *b, float *x);

void EIG_SparseMatrixF_MultiplyWithVector(SparseMatrixF *matrix, float *vector, float *result);
SparseMatrixF *EIG_SparseMatrixF_Transpose(SparseMatrixF *matrix);
SparseMatrixF *EIG_SparseMatrixF_Multiply(SparseMatrixF *a, SparseMatrixF *b);

SparseLeastSquaresSystemF *EIG_SparseLeastSquaresSystemF_FromSystemMatrix(SparseMatrixF *matrix);
void EIG_SparseLeastSquaresSystemF_Delete(SparseLeastSquaresSystemF *system);
void EIG_SparseLeastSquaresSystemF_Solve(SparseLeastSquaresSystemF *system, float *b, float *x);

void EIG_SparseMatrixF_Print(SparseMatrixF *matrix);

#ifdef __cplusplus
}
#endif

#endif /* __EIGEN3_SPARSE_MATRIX_C_API_H__ */