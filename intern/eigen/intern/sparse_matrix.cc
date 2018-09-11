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

#ifndef __EIGEN3_SPARSE_MATRIX_C_API_CC__
#define __EIGEN3_SPARSE_MATRIX_C_API_CC__

#include "sparse_matrix.h"

#include <iostream>
#include <vector>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

struct MatrixFEntries {
	std::vector<Eigen::Triplet<float>> data;
};

struct SparseMatrixF {
	Eigen::SparseMatrix<float> data;

	SparseMatrixF() {}

	SparseMatrixF(Eigen::SparseMatrix<float> data)
		: data(data) {}

	SparseMatrixF(int rows, int columns, MatrixFEntries &entries)
	{
		data = Eigen::SparseMatrix<float>(rows, columns);
		data.setFromTriplets(entries.data.begin(), entries.data.end());
	}
};

struct SparseLDLTDecompositionF {
	Eigen::SparseMatrix<float> L;
	Eigen::VectorXf D;

	SparseLDLTDecompositionF() {}

	SparseLDLTDecompositionF(Eigen::SparseMatrix<float> L, Eigen::VectorXf D)
		: L(L), D(D) {}
};

struct SparseLeastSquaresSystemF {
	SparseLDLTDecompositionF *decomposedATA;
	SparseMatrixF *AT;

	SparseLeastSquaresSystemF(SparseMatrixF *A)
	{
		AT = EIG_SparseMatrixF_Transpose(A);
		SparseMatrixF *ATA = EIG_SparseMatrixF_Multiply(AT, A);
		decomposedATA = EIG_SparseMatrixF_LDLTDecomposition(ATA);
	}

	~SparseLeastSquaresSystemF()
	{
		delete decomposedATA;
		delete AT;
	}

	int rows()
	{
		return AT->data.cols();
	}

	int cols()
	{
		return AT->data.rows();
	}

	int variable_amount()
	{
		return cols();
	}
};

MatrixFEntries *EIG_MatrixFEntries_New()
{
	return new MatrixFEntries();
}

void EIG_MatrixFEntries_Delete(MatrixFEntries *entries)
{
	delete entries;
}

void EIG_SparseMatrixF_Delete(SparseMatrixF *matrix)
{
	delete matrix;
}

void EIG_MatrixFEntries_Add(MatrixFEntries *entries, int row, int column, float value)
{
	entries->data.push_back(Eigen::Triplet<float>(row, column, value));
}

SparseMatrixF *EIG_SparseMatrixF_FromEntries(int rows, int columns, MatrixFEntries *entries)
{
	return new SparseMatrixF(rows, columns, *entries);
}

SparseMatrixF *EIG_SparseMatrixF_LLTDecomposition(SparseMatrixF *matrix)
{
	Eigen::SimplicialLLT<Eigen::SparseMatrix<float>> cholesky(matrix->data);
	return new SparseMatrixF(cholesky.matrixL());
}

SparseLDLTDecompositionF *EIG_SparseMatrixF_LDLTDecomposition(SparseMatrixF *matrix)
{
	Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>> cholesky(matrix->data);
	return new SparseLDLTDecompositionF(cholesky.matrixL(), cholesky.vectorD());
}

void EIG_SparseMatrixF_Print(SparseMatrixF *matrix)
{
	std::cout << Eigen::MatrixXf(matrix->data) << std::endl;
}

void EIG_SparseLowerTriangularSolveF(SparseMatrixF *L, float *b, float *x)
{
	Eigen::SparseMatrix<float> &A = L->data;
	Eigen::VectorXf _b = Eigen::Map<Eigen::VectorXf>(b, A.rows());
	Eigen::VectorXf result = A.triangularView<Eigen::Lower>().solve(_b);
	Eigen::Map<Eigen::VectorXf>(x, A.cols()) = result;
}

void EIG_SparseLLTSolveF(SparseMatrixF *L, float *b, float *x)
{
	Eigen::SparseMatrix<float> &A = L->data;
	Eigen::VectorXf _b = Eigen::Map<Eigen::VectorXf>(b, A.rows());

	Eigen::VectorXf intermediate = A.triangularView<Eigen::Lower>().solve(_b);
	Eigen::VectorXf result = A.transpose().triangularView<Eigen::Upper>().solve(intermediate);

	Eigen::Map<Eigen::VectorXf>(x, A.cols()) = result;
}

void EIG_SparseLDLTSolveF(SparseLDLTDecompositionF *decomposition, float *b, float *x)
{
	Eigen::SparseMatrix<float> &L = decomposition->L;
	Eigen::VectorXf &D = decomposition->D;
	Eigen::VectorXf _b = Eigen::Map<Eigen::VectorXf>(b, L.rows());

	Eigen::VectorXf intermediate = L.triangularView<Eigen::Lower>().solve(_b);
	for (int i = 0; i < intermediate.size(); i++) {
		intermediate[i] /= D[i];
	}
	Eigen::VectorXf result = L.transpose().triangularView<Eigen::Upper>().solve(intermediate);

	Eigen::Map<Eigen::VectorXf>(x, L.cols()) = result;
}

void EIG_SparseLDLTDecompositionF_Delete(SparseLDLTDecompositionF *decomposition)
{
	delete decomposition;
}

void EIG_SparseMatrixF_MultiplyWithVector(SparseMatrixF *matrix, float *vector, float *result)
{
	Eigen::SparseMatrix<float> &A = matrix->data;
	Eigen::VectorXf _vector = Eigen::Map<Eigen::VectorXf>(vector, A.cols());
	Eigen::VectorXf _result = A * _vector;
	Eigen::Map<Eigen::VectorXf>(result, A.rows()) = _result;
}

SparseMatrixF *EIG_SparseMatrixF_Transpose(SparseMatrixF *matrix)
{
	return new SparseMatrixF(matrix->data.transpose());
}

SparseMatrixF *EIG_SparseMatrixF_Multiply(SparseMatrixF *a, SparseMatrixF *b)
{
	return new SparseMatrixF(a->data * b->data);
}

SparseLeastSquaresSystemF *EIG_SparseLeastSquaresSystemF_FromSystemMatrix(SparseMatrixF *matrix)
{
	return new SparseLeastSquaresSystemF(matrix);
}

void EIG_SparseLeastSquaresSystemF_Delete(SparseLeastSquaresSystemF *system)
{
	delete system;
}

void EIG_SparseLeastSquaresSystemF_Solve(SparseLeastSquaresSystemF *system, float *b, float *x)
{
	Eigen::VectorXf _b = Eigen::Map<Eigen::VectorXf>(b, system->variable_amount());
	Eigen::VectorXf ATb = system->AT->data * _b;
	EIG_SparseLDLTSolveF(system->decomposedATA, &ATb[0], x);
}

#endif /* __EIGEN3_SPARSE_MATRIX_C_API_CC__ */