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

#include "BLI_math.h"

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

//#define TIMEIT(name) Timer t(name);
#define TIMEIT(name)

/* ************ Timer End *************** */


typedef Eigen::SparseMatrix<float, Eigen::ColMajor> SparseMatrixF;
typedef Eigen::SparseMatrix<double, Eigen::ColMajor> SparseMatrixD;
typedef Eigen::Triplet<float> Triplet;

struct SolverCache {
	Eigen::SimplicialLDLT<SparseMatrixD> *solver = NULL;
};

struct WeightedEdge {
	int v1, v2;
	float weight;
};

struct SystemMatrixF {
	SparseMatrixF A_II, A_IB;
	/* A_BI: contains only zeros
	 * A_BB: is an identity matrix
	 *  -> don't need to be stored explicitly.
	 */

	std::vector<int> index_of_vertex;
	std::vector<int> vertex_of_index;

	/* edges can exist multiple times, their total weight is the sum */
	std::vector<WeightedEdge> weighted_edges;

	int vertex_amount() { return index_of_vertex.size(); }
	int anchor_amount() { return A_IB.cols(); }
	int inner_amount() { return A_II.rows(); }

	bool is_anchor_vertex(int vertex)
	{
		return !is_inner_vertex(vertex);
	}

	bool is_inner_vertex(int vertex)
	{
		return index_of_vertex[vertex] < inner_amount();
	}

	int get_index_of_vertex(int vertex)
	{
		return index_of_vertex[vertex];
	}
};

static std::vector<WeightedEdge> calcWeightedEdgesFromTriangles_Cotan(
        const MLoopTri *triangles, int triangle_amount, const MLoop *loops, const float (*positions)[3])
{
	std::vector<WeightedEdge> edges;
	edges.reserve(triangle_amount * 3);

	for (int i = 0; i < triangle_amount; i++) {
		const MLoopTri *triangle = triangles + i;
		int v1 = loops[triangle->tri[0]].v;
		int v2 = loops[triangle->tri[1]].v;
		int v3 = loops[triangle->tri[2]].v;

		float angles[3];
		angle_tri_v3(angles, (float *)(positions + v1), (float *)(positions + v2), (float *)(positions + v3));

#define cotan(x) cos(x)/sin(x)
		edges.push_back((WeightedEdge){v2, v3, cotan(angles[0]) / 2.0f});
		edges.push_back((WeightedEdge){v1, v3, cotan(angles[1]) / 2.0f});
		edges.push_back((WeightedEdge){v1, v2, cotan(angles[2]) / 2.0f});
#undef cotan
	}

	return edges;
}

static std::vector<WeightedEdge> calcWeightedEdgesFromTriangles_Uniform(
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

static std::vector<WeightedEdge> calculateEdgeWeights(
        Mesh *mesh /* only for connectivity information */,
        const float (*positions)[3])
{
	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);
	return calcWeightedEdgesFromTriangles_Cotan(triangles, triangle_amount, mesh->mloop, positions);
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

static std::vector<Triplet> getInnerMatrixTriplets(
        const float (*positions)[3], int vertex_amount, std::vector<WeightedEdge> &edges,
        std::vector<int> &anchors, std::vector<int> &index_of_vertex)
{
	int inner_amount = vertex_amount - anchors.size();
	auto total_weights = calcTotalWeigthPerVertex(edges, vertex_amount);

	std::vector<Triplet> triplets;

	for (int i = 0; i < vertex_amount; i++) {
		int index = index_of_vertex[i];
		if (index < inner_amount) {
			triplets.push_back(Triplet(index, index, total_weights[i]));
		}
	}

	for (WeightedEdge edge : edges) {
		if (edge.weight == 0) continue;

		int index1 = index_of_vertex[edge.v1];
		int index2 = index_of_vertex[edge.v2];

		if (index1 < inner_amount) {
			triplets.push_back(Triplet(index1, index2, -edge.weight));
		}
		if (index2 < inner_amount) {
			triplets.push_back(Triplet(index2, index1, -edge.weight));
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
	int inner_amount = vertex_amount - anchor_amount;
	std::vector<int> anchors(anchor_indices, anchor_indices + anchor_amount);

	std::vector<int> vertex_of_index = sortVerticesByAnchors(vertex_amount, anchors);
	std::vector<int> index_of_vertex(vertex_amount);
	for (int i = 0; i < vertex_amount; i++) {
		index_of_vertex[vertex_of_index[i]] = i;
	}

	SystemMatrixF *matrices = new SystemMatrixF();
	matrices->A_II = SparseMatrixF(inner_amount, inner_amount);
	matrices->A_IB = SparseMatrixF(inner_amount,     anchor_amount);
	matrices->index_of_vertex = std::vector<int>(index_of_vertex);
	matrices->vertex_of_index = std::vector<int>(vertex_of_index);
	matrices->weighted_edges = calculateEdgeWeights(mesh, positions);

	std::vector<Triplet> triplets = getInnerMatrixTriplets(positions, vertex_amount, matrices->weighted_edges, anchors, index_of_vertex);
	std::vector<Triplet> triplets_II;
	std::vector<Triplet> triplets_IB;

	for (int i = 0; i < triplets.size(); i++) {
		Triplet triplet = triplets[i];
		if (triplet.col() < inner_amount) {
			triplets_II.push_back(triplet);
		}
		else {
			triplets_IB.push_back(Triplet(triplet.row(), triplet.col() - inner_amount, triplet.value()));
		}
	}

	matrices->A_II.setFromTriplets(triplets_II.begin(), triplets_II.end());
	matrices->A_IB.setFromTriplets(triplets_IB.begin(), triplets_IB.end());

	return (SystemMatrix *)matrices;
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

static std::vector<Eigen::Matrix3f> calculateRotations(
        SystemMatrixF &matrix,
        const float (*positions_before_VO)[3],
        const float (*positions_after_VO)[3])
{
	std::vector<Eigen::Matrix3f> S(matrix.vertex_amount());
	for (int i = 0; i < S.size(); i++) {
		S[i].setZero();
	}

	for (WeightedEdge edge : matrix.weighted_edges) {
		int i = edge.v1;
		int j = edge.v2;
		float weight = edge.weight;

		Eigen::Vector3f edge_old;
		Eigen::RowVector3f edge_new;

		sub_v3_v3v3(&edge_old[0], (float *)(positions_before_VO + i), (float *)(positions_before_VO + j));
		sub_v3_v3v3(&edge_new[0], (float *)(positions_after_VO + i), (float *)(positions_after_VO + j));

		S[i] += weight * edge_old * edge_new;
		S[j] += weight * edge_old * edge_new;
	}

	std::vector<Eigen::Matrix3f> R(S.size());
	for (int i = 0; i < S.size(); i++) {
		Eigen::JacobiSVD<Eigen::Matrix3f> svd(S[i], Eigen::ComputeFullU | Eigen::ComputeFullV);
		R[i] = svd.matrixV() * svd.matrixU().transpose();
	}

	return R;
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

static Eigen::VectorXf solveLaplacianSystem_Single(
        SystemMatrixF &matrix, Eigen::VectorXf &inner_diff_pos,
        Eigen::VectorXf &anchor_pos, SolverCache &cache)
{
	TIMEIT("solve single");
	Eigen::VectorXf b = inner_diff_pos - matrix.A_IB * anchor_pos;
	return solveSparse_NormalEquation(matrix.A_II, b, cache);
}

std::vector<Eigen::Vector3f> updateInnerDiffPos(
        SystemMatrixF &matrix,
        const float (*initial_positions_VO)[3],
        const float (*new_positions_VO)[3],
        const float (*initial_inner_diff_MO)[3])
{
	std::vector<Eigen::Matrix3f> rotations_VO = calculateRotations(matrix, initial_positions_VO, new_positions_VO);
	std::vector<Eigen::Vector3f> new_diffs_MO(matrix.inner_amount());

	for (int i = 0; i < matrix.inner_amount(); i++) {
		new_diffs_MO[i].setZero();
	}

	for (WeightedEdge edge : matrix.weighted_edges) {
		int i_VO = edge.v1;
		int j_VO = edge.v2;
		int i_MO = matrix.get_index_of_vertex(i_VO);
		int j_MO = matrix.get_index_of_vertex(j_VO);
		float weight = edge.weight;

		Eigen::Vector3f old_edge;
		old_edge[0] = initial_positions_VO[i_VO][0] - initial_positions_VO[j_VO][0];
		old_edge[1] = initial_positions_VO[i_VO][1] - initial_positions_VO[j_VO][1];
		old_edge[2] = initial_positions_VO[i_VO][2] - initial_positions_VO[j_VO][2];

		auto value = weight / 2.0f * (rotations_VO[i_VO] + rotations_VO[j_VO]) * old_edge;

		if (i_MO < matrix.inner_amount()) new_diffs_MO[i_MO] += value;
		if (j_MO < matrix.inner_amount()) new_diffs_MO[j_MO] -= value;
	}

	return new_diffs_MO;
}

void solveLaplacianSystem(
        struct SystemMatrix *matrix, const float (*initial_positions_VO)[3],
        const float (*initial_inner_diff_MO)[3], const float (*anchor_pos_MO)[3], SolverCache *cache, int iterations,
        float (*r_result_VO)[3])
{
	TIMEIT("solve all");
	SystemMatrixF& _matrix = *(SystemMatrixF *)matrix;
	int inner_amount = _matrix.inner_amount();
	int anchor_amount = _matrix.anchor_amount();
	int vertex_amount = _matrix.vertex_amount();

	std::vector<Eigen::Vector3f> inner_diffs_MO(inner_amount);
	for (int i = 0; i < inner_amount; i++) {
		copy_v3_v3(&inner_diffs_MO[i][0], (float *)(initial_inner_diff_MO + i));
	}

	for (int iteration = 0; iteration < iterations; iteration++) {
		//std::cout << "Iteration: " << iteration << std::endl;
		for (int coord = 0; coord < 3; coord++) {
			Eigen::VectorXf _inner_diffs_MO(inner_amount);
			for (int i = 0; i < inner_amount; i++) {
				_inner_diffs_MO[i] = inner_diffs_MO[i][coord];
			}

			Eigen::VectorXf _anchor_pos_MO(anchor_amount);
			for (int i = 0; i < anchor_amount; i++) {
				_anchor_pos_MO[i] = anchor_pos_MO[i][coord];
			}

			Eigen::VectorXf inner_result_MO = solveLaplacianSystem_Single(_matrix, _inner_diffs_MO, _anchor_pos_MO, *cache);

			for (int i = 0; i < vertex_amount; i++) {
				int index_MO = _matrix.get_index_of_vertex(i);
				float vertex_value;
				if (index_MO < inner_amount) {
					vertex_value = inner_result_MO[index_MO];
				}
				else {
					vertex_value = anchor_pos_MO[index_MO - inner_amount][coord];
				}
				r_result_VO[i][coord] = vertex_value;
			}
		}
		inner_diffs_MO = updateInnerDiffPos(_matrix, initial_positions_VO, r_result_VO, initial_inner_diff_MO);
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