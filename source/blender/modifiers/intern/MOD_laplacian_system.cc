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
#include <set>
#include <unordered_set>

#include <chrono>
#include <iostream>

/* ************** Timer ***************** */

class Timer {
    const char *name;
    std::chrono::high_resolution_clock::time_point start, end;
    std::chrono::duration<float> duration;

public:
    Timer(const char *name = "");
    ~Timer();
};

Timer::Timer(const char *name) {
    this->name = name;
    this->start = std::chrono::high_resolution_clock::now();
}

Timer::~Timer() {
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    double ms = duration.count() * 1000.0f;
    std::cout << "Timer '" << name << "' took " << ms << " ms" << std::endl;
}

#define TIMEIT(name) Timer t(name);

/* ************ Timer End *************** */


typedef Eigen::SparseMatrix<float> SparseMatrixF;
typedef Eigen::SparseMatrix<double> SparseMatrixD;
typedef Eigen::Triplet<float> Triplet;

struct SystemMatrixF {
	SparseMatrixF A_II, A_IB;
	std::vector<int> index_of_vertex;
	std::vector<int> vertex_of_index;

	int vertex_amount() { return index_of_vertex.size(); }
	int anchor_amount() { return A_IB.cols(); }
	int inner_amount() { return A_II.rows(); }

	/* A_BI: contains only zeros
	 * A_BB: is an identity matrix
	 *  -> don't need to be stored explicitly.
	 */
};

struct SolverCache {
	Eigen::SimplicialLDLT<SparseMatrixD> *solver = NULL;
};

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

static void printSparseMatrix(SparseMatrixF &matrix)
{
	std::cout << std::endl << Eigen::MatrixXf(matrix) << std::endl << std::endl;
}

/* expects the anchor indices to be sorted */
/* (6, [1, 4]) -> [0, 2, 3, 5,  1, 4] */
static std::vector<int> sortVerticesByAnchors(int vertex_amount, std::vector<int> &anchors)
{
	std::vector<int> sorted;

	int anchor_index = 0;
	for (int i = 0; i < vertex_amount; i++) {
		if (anchor_index < anchors.size() && i == anchors[anchor_index]) {
			anchor_index++;
			continue;
		}
		sorted.push_back(i);
	}

	sorted.insert(sorted.end(), anchors.begin(), anchors.end());
	return sorted;
}

static std::vector<Triplet> getInnerMatrixTriplets_TrianglesMode(
        Mesh *mesh, const float (*positions)[3],
        std::vector<int> &anchors, std::vector<int> &index_of_vertex)
{
	int vertex_amount = mesh->totvert;
	int non_anchor_amount = vertex_amount - anchors.size();

	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

	auto edges = calcWeightedEdgesFromTriangles(triangles, triangle_amount, mesh->mloop, positions);
	auto total_weights = calcTotalWeigthPerVertex(edges, mesh->totvert);

	std::vector<Triplet> triplets;

	for (int i = 0; i < non_anchor_amount; i++) {
		triplets.push_back(Triplet(i, i, 1));
	}

	for (WeightedEdge edge : edges) {
		if (edge.weight == 0) continue;
		BLI_assert(total_weights[edge.v1] != 0);
		BLI_assert(total_weights[edge.v2] != 0);

		int index1 = index_of_vertex[edge.v1];
		int index2 = index_of_vertex[edge.v2];

		if (index1 < non_anchor_amount) {
			triplets.push_back(Triplet(index1, index2, -edge.weight / total_weights[edge.v1]));
		}
		if (index2 < non_anchor_amount) {
			triplets.push_back(Triplet(index2, index1, -edge.weight / total_weights[edge.v2]));
		}
	}

	return triplets;
}


SystemMatrix *buildConstraintLaplacianSystemMatrix(
        struct Mesh *mesh /* only used for connectivity information */,
        const float (*positions)[3],
        int *anchor_indices, int anchor_amount)
{
	int vertex_amount = mesh->totvert;
	int non_anchor_amount = vertex_amount - anchor_amount;
	std::vector<int> anchors(anchor_indices, anchor_indices + anchor_amount);

	std::vector<int> vertex_of_index = sortVerticesByAnchors(vertex_amount, anchors);
	std::vector<int> index_of_vertex(vertex_amount);
	for (int i = 0; i < vertex_amount; i++) {
		index_of_vertex[vertex_of_index[i]] = i;
	}

	SystemMatrixF *matrices = new SystemMatrixF();
	matrices->A_II = SparseMatrixF(non_anchor_amount, non_anchor_amount);
	matrices->A_IB = SparseMatrixF(non_anchor_amount,     anchor_amount);
	matrices->index_of_vertex = std::vector<int>(index_of_vertex);
	matrices->vertex_of_index = std::vector<int>(vertex_of_index);

	std::vector<Triplet> triplets = getInnerMatrixTriplets_TrianglesMode(mesh, positions, anchors, index_of_vertex);
	std::vector<Triplet> triplets_II;
	std::vector<Triplet> triplets_IB;

	for (int i = 0; i < triplets.size(); i++) {
		Triplet triplet = triplets[i];
		if (triplet.col() < non_anchor_amount) {
			triplets_II.push_back(triplet);
		}
		else {
			triplets_IB.push_back(Triplet(triplet.row(), triplet.col() - non_anchor_amount, triplet.value()));
		}
	}

	matrices->A_II.setFromTriplets(triplets_II.begin(), triplets_II.end());
	matrices->A_IB.setFromTriplets(triplets_IB.begin(), triplets_IB.end());

	return (SystemMatrix *)matrices;
}

typedef Eigen::Map<Eigen::VectorXf, 0, Eigen::InnerStride<3>> StridedVector;

static Eigen::VectorXf solveSparse_NormalEquation(
        const SparseMatrixF &_A, const Eigen::VectorXf _b, SolverCache &cache)
{
	SparseMatrixD A = _A.cast<double>();
	SparseMatrixD A_T = A.transpose();
	Eigen::VectorXd b = _b.cast<double>();

	if (cache.solver == NULL) {
		cache.solver = new Eigen::SimplicialLDLT<SparseMatrixD>();
		cache.solver->compute(A_T * A);
	}
	{
		return cache.solver->solve(A_T * b).cast<float>();
	}
}

SolverCache *SolverCache_new()
{
	return new SolverCache();
}

void SolverCache_delete(struct SolverCache *cache)
{
	delete cache;
}

void SolverCache_matrix_changed(SolverCache *cache)
{
	delete cache->solver;
	cache->solver = NULL;
}

static Eigen::VectorXf solveLaplacianSystem_Single(
        SystemMatrixF &matrix, Eigen::VectorXf &inner_diff_pos,
        Eigen::VectorXf &anchor_pos, SolverCache &cache)
{
	TIMEIT("solve single");
	Eigen::VectorXf b = inner_diff_pos - matrix.A_IB * anchor_pos;
	return solveSparse_NormalEquation(matrix.A_II, b, cache);
}

void solveLaplacianSystem(
        struct SystemMatrix *matrix,
        const float (*inner_diff_pos)[3], const float (*anchor_pos)[3], SolverCache *cache,
        float (*r_result)[3])
{
	TIMEIT("solve all");

	SystemMatrixF *_matrix = (SystemMatrixF *)matrix;
	int inner_amount = _matrix->inner_amount();
	int anchor_amount = _matrix->anchor_amount();
	int vertex_amount = _matrix->vertex_amount();

	for (int coord = 0; coord < 3; coord++) {
		Eigen::VectorXf _inner_diff_pos = StridedVector((float *)inner_diff_pos + coord, inner_amount);
		Eigen::VectorXf _anchor_pos = StridedVector((float *)anchor_pos + coord, anchor_amount);
		Eigen::VectorXf inner_result = solveLaplacianSystem_Single(*_matrix, _inner_diff_pos, _anchor_pos, *cache);
		Eigen::VectorXf full_result(vertex_amount);
		{
			TIMEIT("copy back");
			for (int i = 0; i < vertex_amount; i++) {
				int index = _matrix->index_of_vertex[i];
				if (index < inner_amount) {
					full_result[i] = inner_result[index];
				}
				else {
					full_result[i] = anchor_pos[index - inner_amount][coord];
				}
			}
			StridedVector((float *)r_result + coord, vertex_amount) = full_result;
		}
	}
}

void calculateInitialInnerDiff(
        struct SystemMatrix *system_matrix,
        float (*positions)[3],
        float (*r_inner_diff)[3])
{
	TIMEIT("initial inner diff");
	SystemMatrixF *_matrix = (SystemMatrixF *)system_matrix;
	int vertex_amount = _matrix->vertex_amount();
	int inner_amount = _matrix->inner_amount();
	int anchor_amount = _matrix->anchor_amount();

	for (int coord = 0; coord < 3; coord++) {
		Eigen::VectorXf _vector = StridedVector((float *)positions + coord, vertex_amount);
		Eigen::VectorXf _sorted_vector(vertex_amount);
		for (int i = 0; i < vertex_amount; i++) {
			_sorted_vector[_matrix->index_of_vertex[i]] = _vector[i];
		}
		Eigen::VectorXf _result =   _matrix->A_II * _sorted_vector.segment(0, inner_amount)
		                          + _matrix->A_IB * _sorted_vector.segment(inner_amount, anchor_amount);
		StridedVector((float *)r_inner_diff + coord, inner_amount) = _result;
	}
}


/*
Input: Original Vertex Positions, Mesh Connectivity, Anchor Indices, New Anchor Positions

Original Free Differential Coordinates: Original Vertex Positions, Mesh Connectivity
Rotation Matrices <- Final Vertex Positions, Original Vertex Positions
Target Free Differential Coordinates: Original Free Differential Coordinates, Rotation Matrices
b_B <- New Anchor Positions
b_I <- Target Free Differential Coordinates
A_IB <- Original Vertex Positions, Mesh Connectivity, Anchor Indices
A_II <- Original Vertex Positions, Mesh Connectivity, Anchor Indices
New Free Positions <- solve for x_I in    A_II * x_I = b_I - A_IB * b_B
New Anchor Positions <- New Vertex Positions, Anchor Indices
Final Vertex Positions <- New Anchor Positions, New Free Positions
*/