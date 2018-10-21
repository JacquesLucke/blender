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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 by the Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "Eigen/Sparse"
#include "Eigen/Dense"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh_runtime.h"

#include "MOD_laplacian_system.h"

#include <iostream>
#include <vector>

typedef Eigen::SparseMatrix<float, Eigen::RowMajor> SparseMatrixF;
typedef Eigen::Triplet<float> Triplet;

struct WeightedEdge {
	int v1, v2;
	float weight;
};

static std::vector<WeightedEdge> calcWeightedEdgesFromTriangles(
        const MLoopTri *triangles, int triangle_amount, const MLoop *loops, const float (*positions)[3])
{
	std::vector<WeightedEdge> edges;
	edges.reserve(triangle_amount * 3);

	for (int i = 0; i < triangle_amount; i++) {
		const MLoopTri *triangle = triangles + i;
		int v1 = loops[triangle->tri[0]].v;
		int v2 = loops[triangle->tri[1]].v;
		int v3 = loops[triangle->tri[2]].v;

		edges.push_back((WeightedEdge){v1, v2, 1});
		edges.push_back((WeightedEdge){v2, v3, 1});
		edges.push_back((WeightedEdge){v3, v1, 1});
	}

	return edges;
}

static std::vector<float> calcTotalWeigthPerVertex(std::vector<WeightedEdge> &edges, int vertex_amount)
{
	std::vector<float> total_weights(vertex_amount, 0);
	for (WeightedEdge edge : edges) {
		total_weights[edge.v1] += edge.weight;
		total_weights[edge.v2] += edge.weight;
	}
	return total_weights;
}


static std::vector<Triplet> getLaplaceTriplets_TrianglesMode(Mesh *mesh, const float (*positions)[3])
{
	int vertex_amount = mesh->totvert;

	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

	auto edges = calcWeightedEdgesFromTriangles(triangles, triangle_amount, mesh->mloop, positions);
	auto total_weights = calcTotalWeigthPerVertex(edges, mesh->totvert);

	std::vector<Triplet> triplets;

	for (int i = 0; i < vertex_amount; i++) {
		triplets.push_back(Triplet(i, i, 1));
	}

	for (WeightedEdge edge : edges) {
		if (edge.weight == 0) continue;
		BLI_assert(total_weights[edge.v1] != 0);
		BLI_assert(total_weights[edge.v2] != 0);

		triplets.push_back(Triplet(edge.v1, edge.v2, -edge.weight / total_weights[edge.v1]));
		triplets.push_back(Triplet(edge.v2, edge.v1, -edge.weight / total_weights[edge.v2]));
	}

	return triplets;
}

static void clearRowsExceptDiagonal(SparseMatrixF &matrix, std::vector<int> &indices_to_zero)
{
	BLI_assert(matrix.IsRowMajor);

	float *values = matrix.valuePtr();
	int *starts = matrix.outerIndexPtr();
	int *indices = matrix.innerIndexPtr();

	for (int index : indices_to_zero) {
		BLI_assert(starts[index] < starts[index + 1]);

		for (int i = starts[index]; i < starts[index + 1]; i++) {
			if (indices[i] != index) values[i] = 0.0f;
		}
	}
}


SparseMatrix *buildLaplacianSystemMatrix(
        Mesh *mesh /* only used for connectivity information */,
        const float (*positions)[3],
        int *anchor_indices, int anchor_amount)
{
	int vertex_amount = mesh->totvert;
	SparseMatrixF matrix(vertex_amount, vertex_amount);

	std::vector<Triplet> triplets = getLaplaceTriplets_TrianglesMode(mesh, positions);
	matrix.setFromTriplets(triplets.begin(), triplets.end());

	std::vector<int> anchors(anchor_indices, anchor_indices + anchor_amount);
	clearRowsExceptDiagonal(matrix, anchors);

	return (SparseMatrix *)new SparseMatrixF(matrix);
}

void multipleSparseMatrixAndVector(SparseMatrix *matrix, float *vector, float *r_vector)
{
	SparseMatrixF& _matrix = *(SparseMatrixF *)matrix;
	Eigen::VectorXf _vector = Eigen::Map<Eigen::VectorXf>(vector, _matrix.cols());
	Eigen::VectorXf _result = _matrix * _vector;
	Eigen::Map<Eigen::VectorXf>(r_vector, _matrix.rows()) = _result;
}

int getSparseMatrixColumnAmount(struct SparseMatrix *matrix)
{
	return ((SparseMatrixF *)matrix)->cols();
}

void solveSparseSystem(SparseMatrix *A, float *b, float *r_x)
{
	SparseMatrixF& matrix = *(SparseMatrixF *)A;
	Eigen::VectorXf _b = Eigen::Map<Eigen::VectorXf>(b, matrix.cols());

	Eigen::SparseLU<SparseMatrixF> solver;
	solver.compute(matrix);
	Eigen::VectorXf result = solver.solve(_b);

	Eigen::Map<Eigen::VectorXf>(r_x, matrix.rows()) = result;
}