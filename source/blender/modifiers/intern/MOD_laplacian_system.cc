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

#define TIMEIT(name) Timer t(name);

/* ************ Timer End *************** */


typedef Eigen::Map<Eigen::VectorXf, 0, Eigen::InnerStride<3>> StridedVector;
typedef Eigen::SparseMatrix<float, Eigen::ColMajor> SparseMatrixF;
typedef Eigen::SparseMatrix<double, Eigen::ColMajor> SparseMatrixD;
typedef Eigen::Triplet<float> Triplet;

struct WeightedEdge {
	int v1, v2;
	float weight;
};


class Vectors {
	Eigen::VectorXf data;

public:
	Vectors() {}

	Vectors(int size)
	{
		this->data = Eigen::VectorXf(size * 3);
	}

	Vectors(std::vector<Eigen::Vector3f> &vectors)
		: Vectors((Vector3Ds)&vectors[0][0], vectors.size()) {}

	Vectors(Vector3Ds vectors, int amount)
	{
		this->data = Eigen::VectorXf(3 * amount);
		memcpy(&this->data[0], vectors, this->byte_size());
	}

	void set_zero()
	{
		this->data.setZero();
	}

	StridedVector get_coord(int coord)
	{
		return StridedVector(this->data.data() + coord, this->size());
	}

	void set_coord(int coord, Eigen::VectorXf &values)
	{
		BLI_assert(values.size() == this->size());
		this->get_coord(coord) = values;
	}

	void copy_to(Vector3Ds dst)
	{
		memcpy(dst, &this->data[0], this->byte_size());
	}

	float *get_vector_ptr(int index)
	{
		return this->ptr() + (3 * index);
	}

	void set_vector_ptr(int index, float *vector)
	{
		memcpy(this->ptr() + (3 * index), vector, sizeof(float) * 3);
	}

	Eigen::Map<Eigen::Vector3f> operator [](int index)
	{
		return Eigen::Map<Eigen::Vector3f>(this->get_vector_ptr(index));
	}

	float *ptr()
	{
		return &this->data[0];
	}

	int size()
	{
		return this->data.size() / 3;
	}

	int byte_size()
	{
		return this->size() * 3 * sizeof(float);
	}

	void print(std::string name){
		std::cout << name << ":" << std::endl;
		for (int i = 0; i < this->size(); i++) {
			float *vector = this->get_vector_ptr(i);
			printf("  %7.3f %7.3f %7.3f\n", vector[0], vector[1], vector[2]);
		}
	}
};

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

static Vectors getVertexPositions(Mesh *mesh)
{
	std::vector<Eigen::Vector3f> positions;

	for (int i = 0; i < mesh->totvert; i++) {
		Eigen::Vector3f position;
		copy_v3_v3(&position[0], mesh->mvert[i].co);
		positions.push_back(position);
	}

	return positions;
}

static std::vector<std::array<int, 3>> getTriangleIndices(Mesh *mesh)
{
	std::vector<std::array<int, 3>> indices;

	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

	for (int i = 0; i < triangle_amount; i++) {
		int v1 = mesh->mloop[triangles[i].tri[0]].v;
		int v2 = mesh->mloop[triangles[i].tri[1]].v;
		int v3 = mesh->mloop[triangles[i].tri[2]].v;
		indices.push_back({v1, v2, v3});
	}

	return indices;
}

struct ReorderData
{
private:
	std::vector<int> orig_to_new;
	std::vector<int> new_to_orig;
	int _inner_amount;

public:

	ReorderData() {}

	ReorderData(std::vector<int> &anchors, int vertex_amount)
	{
		this->new_to_orig = sortVerticesByAnchors(vertex_amount, anchors);

		this->orig_to_new.resize(vertex_amount);
		for (int i = 0; i < vertex_amount; i++) {
			orig_to_new[new_to_orig[i]] = i;
		}

		this->_inner_amount = vertex_amount - anchors.size();
	}

	int inner_amount()
	{
		return this->_inner_amount;
	}

	bool is_inner__orig(int index)
	{
		return this->orig_to_new[index] < this->_inner_amount;
	}

	bool is_inner__new(int index)
	{
		return index < this->_inner_amount;
	}

	int to_orig(int index)
	{
		return this->new_to_orig[index];
	}

	int to_new(int index)
	{
		return this->orig_to_new[index];
	}

	int to_new_anchor(int index)
	{
		return this->to_new(index) - this->inner_amount();
	}
};

static std::vector<WeightedEdge> calculateEdgeWeights_FromTriangles_Cotan(
        Vectors &positions, std::vector<std::array<int, 3>> &triangles)
{
	std::vector<WeightedEdge> edges(triangles.size() * 3);

	for (auto verts : triangles) {
		float angles[3];
		angle_tri_v3(angles,
		        positions.get_vector_ptr(verts[0]),
		        positions.get_vector_ptr(verts[1]),
		        positions.get_vector_ptr(verts[2]));

#define cotan(x) cos((x))/sin((x))
		edges.push_back((WeightedEdge){verts[1], verts[2], cotan(angles[0]) / 2.0f});
		edges.push_back((WeightedEdge){verts[0], verts[2], cotan(angles[1]) / 2.0f});
		edges.push_back((WeightedEdge){verts[0], verts[1], cotan(angles[2]) / 2.0f});
#undef cotan
	}

	return edges;
}

static std::vector<Triplet> getLaplaceMatrixTriplets(
        int vertex_amount, std::vector<WeightedEdge> &edges)
{
	auto total_weights = calcTotalWeigthPerVertex(edges, vertex_amount);

	std::vector<Triplet> triplets;

	for (int i = 0; i < vertex_amount; i++) {
		triplets.push_back(Triplet(i, i, total_weights[i]));
	}

	for (WeightedEdge edge : edges) {
		if (edge.weight == 0) continue;

		triplets.push_back(Triplet(edge.v1, edge.v2, -edge.weight));
		triplets.push_back(Triplet(edge.v2, edge.v1, -edge.weight));
	}

	return triplets;
}

static std::vector<Eigen::Matrix3f> calculate_rotations(
        std::vector<WeightedEdge> &edges, Vectors &initial, Vectors &new_inner, Vectors &anchors, ReorderData &order)
{
	BLI_assert(initial.size() == new_inner.size() + anchors.size());

	std::vector<Eigen::Matrix3f> S(initial.size());
	for (int i = 0; i < S.size(); i++) S[i].setZero();

	for (WeightedEdge edge : edges) {
		int v1 = edge.v1;
		int v2 = edge.v2;
		bool v1_is_inner = order.is_inner__orig(v1);
		bool v2_is_inner = order.is_inner__orig(v2);

		Eigen::Vector3f edge_old = initial[v1] - initial[v2];

		Eigen::Vector3f edge_new_start = v1_is_inner ? new_inner[order.to_new(v1)] : anchors[order.to_new_anchor(v1)];
		Eigen::Vector3f edge_new_end   = v2_is_inner ? new_inner[order.to_new(v2)] : anchors[order.to_new_anchor(v2)];
		Eigen::RowVector3f edge_new = edge_new_start - edge_new_end;

		Eigen::Matrix3f mat = edge.weight * edge_old * edge_new;
		S[v1] += mat;
		S[v2] += mat;
	}

	std::vector<Eigen::Matrix3f> R(S.size());
	for (int i = 0; i < S.size(); i++) {
		Eigen::JacobiSVD<Eigen::Matrix3f> svd(S[i], Eigen::ComputeFullU | Eigen::ComputeFullV);
		R[i] = svd.matrixV() * svd.matrixU().transpose();
	}

	return R;
}

static Vectors calculate_new_inner_diff(
        std::vector<WeightedEdge> &edges, Vectors &initial, Vectors &new_inner, Vectors &anchors, ReorderData &order)
{
	Vectors new_diffs(order.inner_amount());
	new_diffs.set_zero();

	auto rotations = calculate_rotations(edges, initial, new_inner, anchors, order);

	for (WeightedEdge edge : edges) {
		int v1 = edge.v1;
		int v2 = edge.v2;
		bool v1_is_inner = order.is_inner__orig(v1);
		bool v2_is_inner = order.is_inner__orig(v2);
		if (!(v1_is_inner || v2_is_inner)) continue;

		Eigen::Vector3f old_edge = initial[v1] - initial[v2];
		Eigen::Vector3f value = edge.weight / 2.0f * (rotations[v1] + rotations[v2]) * old_edge;

		if (v1_is_inner) new_diffs[order.to_new(v1)] += value;
		if (v2_is_inner) new_diffs[order.to_new(v2)] -= value;
	}

	return new_diffs;
}

struct LaplacianSystemMatrix
{
	SparseMatrixF L, A_II, A_IB;
	ReorderData order;
	Eigen::SimplicialLDLT<SparseMatrixD> *solver;

	LaplacianSystemMatrix(
	        std::vector<WeightedEdge> &edges,
	        std::vector<int> anchors,
			int vertex_amount)
	{
		int anchor_amount = anchors.size();
		int inner_amount = vertex_amount - anchor_amount;

		this->order = ReorderData(anchors, vertex_amount);

		std::vector<Triplet> laplace_triplets = getLaplaceMatrixTriplets(vertex_amount, edges);
		std::vector<Triplet> triplets_A_II, triplets_A_IB;

		for (Triplet triplet : laplace_triplets) {
			int reorder_row = this->order.to_new(triplet.row());
			int reorder_col = this->order.to_new(triplet.col());

			if (reorder_row < inner_amount) {
				if (reorder_col < inner_amount) {
					triplets_A_II.push_back(Triplet(reorder_row, reorder_col, triplet.value()));
				} else {
					triplets_A_IB.push_back(Triplet(reorder_row, reorder_col - inner_amount, triplet.value()));
				}
			}
		}

		this->A_II = SparseMatrixF(inner_amount, inner_amount);
		this->A_IB = SparseMatrixF(inner_amount, anchor_amount);
		this->L = SparseMatrixF(vertex_amount, vertex_amount);
		this->A_II.setFromTriplets(triplets_A_II.begin(), triplets_A_II.end());
		this->A_IB.setFromTriplets(triplets_A_IB.begin(), triplets_A_IB.end());
		this->L.setFromTriplets(laplace_triplets.begin(), laplace_triplets.end());

		this->solver = new Eigen::SimplicialLDLT<SparseMatrixD>();
		this->solver->compute(this->A_II.cast<double>().transpose() * this->A_II.cast<double>());
	}

	int vertex_amount()
	{
		return this->A_II.cols() + this->A_IB.cols();
	}

	int inner_amount()
	{
		return this->A_II.cols();
	}

	int anchor_amount()
	{
		return this->vertex_amount() - this->inner_amount();
	}

	Eigen::VectorXf calculateInnerDiff_SingleCoord(Eigen::VectorXf positions)
	{
		int vertex_amount = this->vertex_amount();
		int inner_amount = this->inner_amount();
		int anchor_amount = this->anchor_amount();

		Eigen::VectorXf sorted_vector(vertex_amount);
		for (int i = 0; i < vertex_amount; i++) {
			sorted_vector[this->order.to_new(i)] = positions[i];
		}
		Eigen::VectorXf result =
			  this->A_II * sorted_vector.segment(0, inner_amount)
			+ this->A_IB * sorted_vector.segment(inner_amount, anchor_amount);

		return result;
	}

	Vectors *calculateInnerDiff(Vectors &positions)
	{
		int inner_amount = this->inner_amount();
		Vectors *output = new Vectors(inner_amount);

		for (int coord = 0; coord < 3; coord++) {
			Eigen::VectorXf vector = positions.get_coord(coord);
			Eigen::VectorXf result = calculateInnerDiff_SingleCoord(vector);
			output->set_coord(coord, result);
		}

		return output;
	}

	Eigen::VectorXf solve_single_coord(StridedVector initial_inner_diff, StridedVector anchor_positions)
	{
		Eigen::VectorXf b = initial_inner_diff - this->A_IB * anchor_positions;
		Eigen::VectorXf result = this->solver->solve(this->A_II.cast<double>().transpose() * b.cast<double>()).cast<float>();

		return result;
	}

	Vectors solve(Vectors &initial_inner_diff, Vectors &anchor_positions)
	{
		Vectors output(this->inner_amount());
		for (int coord = 0; coord < 3; coord++) {
			Eigen::VectorXf single_result = this->solve_single_coord(
			        initial_inner_diff.get_coord(coord),
			        anchor_positions.get_coord(coord));
			output.set_coord(coord, single_result);
		}
		return output;
	}
};

class LaplacianSystem
{

private:
	Vectors orig_vertex_positions;
	std::vector<std::array<int, 3>> triangle_indices;
	std::vector<WeightedEdge> edges;

	std::vector<int> *anchor_indices = nullptr;
	LaplacianSystemMatrix *system_matrix = nullptr;
	Vectors *initial_inner_diff = nullptr;

public:
	LaplacianSystem(Mesh *orig_mesh)
	{
		this->orig_vertex_positions = getVertexPositions(orig_mesh);
		this->triangle_indices = getTriangleIndices(orig_mesh);
		this->edges = calculateEdgeWeights_FromTriangles_Cotan(
		        this->orig_vertex_positions,
		        this->triangle_indices);
	}

	void setAnchors(std::vector<int> &anchor_indices)
	{
		this->anchor_indices = new std::vector<int>(anchor_indices);
		this->system_matrix = new LaplacianSystemMatrix(
		        this->edges, *this->anchor_indices, this->vertex_amount());
		this->initial_inner_diff = this->system_matrix->calculateInnerDiff(this->orig_vertex_positions);
	}

	Vectors calculate_inner_coordinates(Vectors &anchor_positions, int iterations)
	{
		Vectors inner_diff = *this->initial_inner_diff;
		Vectors result;

		for (int i = 0; i < iterations; i++) {
			result = this->system_matrix->solve(inner_diff, anchor_positions);
			inner_diff = calculate_new_inner_diff(this->edges, this->orig_vertex_positions, result, anchor_positions, this->system_matrix->order);
		}

		return result;
	}

	void correct_non_anchors(Vectors &positions, int iterations)
	{
		Vectors anchors = this->extract_anchor_positions(positions);
		Vectors new_inner_coords = this->calculate_inner_coordinates(anchors, iterations);
		this->writeback_inner_postions(positions, new_inner_coords);
	}

	Vectors extract_anchor_positions(Vectors &all_positions)
	{
		Vectors anchors(this->anchor_amount());
		for (int i = 0; i < this->anchor_indices->size(); i++) {
			int index = (*this->anchor_indices)[i];
			anchors.set_vector_ptr(i, all_positions.get_vector_ptr(index));
		}
		return anchors;
	}

	void writeback_inner_postions(Vectors &all_positions, Vectors &inner_positions)
	{
		for (int i = 0; i < inner_positions.size(); i++) {
			int i_orig = this->system_matrix->order.to_orig(i);
			all_positions.set_vector_ptr(i_orig, inner_positions.get_vector_ptr(i));
		}
	}

	int vertex_amount()
	{
		return this->orig_vertex_positions.size();
	}

	int anchor_amount()
	{
		return this->anchor_indices->size();
	}

	int inner_amount()
	{
		return this->vertex_amount() - this->anchor_amount();
	}
};

LaplacianSystem *LaplacianSystem_new(struct Mesh *mesh)
{
	TIMEIT("new");
	return new LaplacianSystem(mesh);
}

void LaplacianSystem_setAnchors(
        LaplacianSystem *system,
        int *anchor_indices, int anchor_amount)
{
	TIMEIT("set anchors");
	std::vector<int> anchors(anchor_indices, anchor_indices + anchor_amount);
	system->setAnchors(anchors);
}

void LaplacianSystem_correctNonAnchors(
        LaplacianSystem *system, Vector3Ds positions, int iterations)
{
	Vectors _positions(positions, system->vertex_amount());
	system->correct_non_anchors(_positions, iterations);
	_positions.copy_to(positions);
}