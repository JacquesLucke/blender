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

#include "MOD_rigiddeform_system.hpp"
#include "BLI_math.h"

#include <chrono>

/* Timer
 **************************************/

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


namespace RigidDeform {

	static void print_sparse_matrix(SparseMatrixD &A)
	{
		std::cout << Eigen::MatrixXd(A) << std::endl;
	}

	/* Build Laplace Matrix
	******************************************/

	static std::vector<double> calc_total_weight_per_vertex(
			const WeightedEdges &edges,
			uint vertex_amount)
	{
		std::vector<double> total_weights(vertex_amount, 0);
		for (WeightedEdge edge : edges) {
			total_weights[edge.v1] += edge.weight;
			total_weights[edge.v2] += edge.weight;
		}
		return total_weights;
	}

	static std::array<double, 3> triangle_corner_angles(
		Eigen::Vector3d v1, Eigen::Vector3d v2, Eigen::Vector3d v3)
	{
		Eigen::Vector3f v1_f = v1.cast<float>();
		Eigen::Vector3f v2_f = v2.cast<float>();
		Eigen::Vector3f v3_f = v3.cast<float>();

		float angles[3];
		angle_tri_v3(angles, v1_f.data(), v2_f.data(), v3_f.data());
		return {angles[0], angles[1], angles[2]};
	}

	static inline double cotan(double angle)
	{
		return std::cos(angle) / std::sin(angle);
	}

	static WeightedEdges calculate_cotan_edge_weights(
		const Vectors &positions,
		const std::vector<std::array<uint, 3>> &triangles)
	{
		WeightedEdges edges;

		for (auto verts : triangles) {
			std::array<double, 3> angles = triangle_corner_angles(
				positions[verts[0]],
				positions[verts[1]],
				positions[verts[2]]);

			double w1 = (angles[0] > 0.0001) ? cotan(angles[0]) / 2.0 : 1.0;
			double w2 = (angles[0] > 0.0001) ? cotan(angles[1]) / 2.0 : 1.0;
			double w3 = (angles[0] > 0.0001) ? cotan(angles[2]) / 2.0 : 1.0;

			if (w1 > 0.0001) edges.push_back(WeightedEdge(verts[1], verts[2], w1));
			if (w2 > 0.0001) edges.push_back(WeightedEdge(verts[0], verts[2], w2));
			if (w3 > 0.0001) edges.push_back(WeightedEdge(verts[0], verts[1], w3));
		}

		return edges;
	}

	static Triplets get_laplace_matrix_triplets(
		uint vertex_amount,
		const WeightedEdges &edges)
	{
		auto total_weights = calc_total_weight_per_vertex(edges, vertex_amount);

		Triplets triplets;

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


	/* Initialize
	 *****************************************/

	RigidDeformSystem::RigidDeformSystem(
		const Vectors &initial_positions,
		const std::vector<std::array<uint, 3>> &triangles)
	{
		m_initial_positions = initial_positions;
		m_edges = calculate_cotan_edge_weights(initial_positions, triangles);
		m_laplace_triplets = get_laplace_matrix_triplets(this->vertex_amount(), m_edges);

#if USE_CHOLUP
		m_laplace_matrix = SparseMatrixD(m_initial_positions.size(), m_initial_positions.size());
		m_laplace_matrix.setFromTriplets(m_laplace_triplets.begin(), m_laplace_triplets.end());
		CholUp::SparseMatrix<double> L(m_laplace_matrix);
		m_solver = std::unique_ptr<Solver>(new Solver(L));
#endif
	}


	/* Set Anchors
	 ******************************************/

	void RigidDeformSystem::set_anchors(
		const std::vector<uint> &anchor_indices)
	{
		m_order = ReorderData(anchor_indices, this->vertex_amount());
		m_anchor_indices = anchor_indices;

		this->update_inner_indices();
		this->update_impact_data();
		this->update_matrix();

#if USE_CHOLUP
		CholUp::SparseMatrix<double> L(m_laplace_matrix);
		std::vector<int> indices(m_inner_indices.begin(), m_inner_indices.end());
		m_solver_current = m_solver->dirichletPartialFactor(L, indices);
#else
		m_solver = std::unique_ptr<Solver>(new Solver());
		m_solver->compute(m_A_II);
#endif
	}

	void RigidDeformSystem::update_inner_indices()
	{
		m_inner_indices = {};
		for (uint i = 0; i < this->vertex_amount(); i++) {
			if (m_order.is_inner__orig(i)) {
				m_inner_indices.push_back(i);
			}
		}
	}

	void RigidDeformSystem::update_matrix()
	{
		Triplets triplets_A_II, triplets_A_IB;
		for (Triplet triplet : m_laplace_triplets) {
			if (m_order.is_inner__orig(triplet.row())) {
				uint reorder_row = m_order.to_new(triplet.row());
				if (m_order.is_inner__orig(triplet.col())) {
					triplets_A_II.push_back(Triplet(
						reorder_row,
						m_order.to_new(triplet.col()),
						triplet.value()));
				}
				else {
					triplets_A_IB.push_back(Triplet(
						reorder_row,
						m_order.to_new_anchor(triplet.col()),
						triplet.value()));
				}
			}
		}

		m_A_II = SparseMatrixD(m_order.inner_amount(), m_order.inner_amount());
		m_A_IB = SparseMatrixD(m_order.inner_amount(), m_order.anchor_amount());
		m_A_II.setFromTriplets(triplets_A_II.begin(), triplets_A_II.end());
		m_A_IB.setFromTriplets(triplets_A_IB.begin(), triplets_A_IB.end());
	}

	void RigidDeformSystem::update_impact_data()
	{
		m_impact.m_edges = {};
		std::vector<bool> has_impact = {};
		this->get_impact_edges(m_impact.m_edges, has_impact);

		m_impact.m_compact_amount = 0;
		m_impact.m_compact_map.resize(this->vertex_amount());
		for (uint i = 0; i < this->vertex_amount(); i++) {
			if (has_impact[i]) {
				m_impact.m_compact_map[i] = m_impact.m_compact_amount;
				m_impact.m_compact_amount++;
			}
			else {
				m_impact.m_compact_map[i] = -1;
			}
		}
	}

	void RigidDeformSystem::get_impact_edges(
		WeightedEdges &r_impact_edges,
		std::vector<bool> &r_vertex_has_impact)
	{
		std::vector<bool> &has_impact = r_vertex_has_impact;
		has_impact.resize(this->vertex_amount(), false);

		for (WeightedEdge edge : m_edges) {
			if (m_order.is_inner__orig(edge.v1) || m_order.is_inner__orig(edge.v2)) {
				has_impact[edge.v1] = true;
				has_impact[edge.v2] = true;
			}
		}
		for (WeightedEdge edge : m_edges) {
			if (has_impact[edge.v1] || has_impact[edge.v2]) {
				r_impact_edges.push_back(edge);
				has_impact[edge.v1] = true;
				has_impact[edge.v2] = true;
			}
		}
	}


	/* Solve Inner Positions
	 ********************************************/

	Vectors RigidDeformSystem::calculate_inner(
		const Vectors &anchor_positions,
		uint iterations)
	{
		assert(iterations > 0);
		assert(this->anchor_indices().size() > 0);

		std::vector<Eigen::Matrix3d> rotations(this->vertex_amount());
		std::fill(rotations.begin(), rotations.end(), Eigen::Matrix3d::Identity());

		const std::array<Eigen::VectorXd, 3> b_preprocessed = {
			m_A_IB * anchor_positions.get_coord(0),
			m_A_IB * anchor_positions.get_coord(1),
			m_A_IB * anchor_positions.get_coord(2),
		};

		uint iteration = 0;
		while (true) {
			iteration++;
			Vectors new_inner_positions = this->optimize_inner_positions(b_preprocessed, rotations);
			if (iteration == iterations) {
				return new_inner_positions;
			}
			rotations = this->optimize_rotations(anchor_positions, new_inner_positions);
		}
	}


	/* Optimize Rotations
	*********************************************/

	static inline Eigen::Vector3d get_position(
		uint index,
		const ReorderData &order,
		const Vectors &anchor_positions,
		const Vectors &inner_positions)
	{
		if (order.is_inner__orig(index)) {
			return inner_positions[order.to_new(index)];
		}
		else {
			return anchor_positions[order.to_new_anchor(index)];
		}
	}

	std::vector<Eigen::Matrix3d> RigidDeformSystem::optimize_rotations(
		const Vectors &anchor_positions,
		const Vectors &new_inner_positions)
	{
		std::vector<Eigen::Matrix3d> S(m_impact.compact_amount());
		for (uint i = 0; i < S.size(); i++) S[i].setZero();

		for (WeightedEdge edge : m_impact.edges()) {
			uint v1 = edge.v1;
			uint v2 = edge.v2;

			Eigen::Vector3d edge_old = m_initial_positions[v1] - m_initial_positions[v2];

			Eigen::Vector3d edge_new_start =
				get_position(v1, m_order, anchor_positions, new_inner_positions);
			Eigen::Vector3d edge_new_end =
				get_position(v2, m_order, anchor_positions, new_inner_positions);

			Eigen::RowVector3d edge_new = edge_new_start - edge_new_end;

			Eigen::Matrix3d mat = edge.weight * edge_old * edge_new;
			S[m_impact.compact_index(v1)] += mat;
			S[m_impact.compact_index(v2)] += mat;
		}

		std::vector<Eigen::Matrix3d> R(S.size());
		for (int i = 0; i < S.size(); i++) {
			Eigen::JacobiSVD<Eigen::Matrix3d> svd(S[i], Eigen::ComputeFullU | Eigen::ComputeFullV);
			R[i] = svd.matrixV() * svd.matrixU().transpose();
			if (R[i].determinant() < 0) {
				Eigen::Matrix3d U = svd.matrixU();
				U.col(2) = -U.col(2);
				R[i] = svd.matrixV() * U.transpose();
			}
		}

		return R;
	}


	/* Optimize Inner Vertex Positions
	**********************************************/

	Vectors RigidDeformSystem::optimize_inner_positions(
		const std::array<Eigen::VectorXd, 3> &b_preprocessed,
		const std::vector<Eigen::Matrix3d> &rotations)
	{
		Vectors new_inner_diffs = this->calculate_new_inner_diffs(rotations);
		return this->solve_for_new_inner_positions(b_preprocessed, new_inner_diffs);
	}

	Vectors RigidDeformSystem::calculate_new_inner_diffs(
		const std::vector<Eigen::Matrix3d> &rotations)
	{
		Vectors new_inner_diffs(m_order.inner_amount());
		new_inner_diffs.set_zero();

		for (WeightedEdge edge : m_impact.edges()) {
			uint v1 = edge.v1;
			uint v2 = edge.v2;
			int v1_compact = m_impact.compact_index(v1);
			int v2_compact = m_impact.compact_index(v2);
			assert(v1_compact >= 0 && v2_compact >= 0);

			Eigen::Vector3d old_edge = m_initial_positions[v1] - m_initial_positions[v2];
			Eigen::Vector3d value =
				edge.weight / 2.0f * (rotations[v1_compact] + rotations[v2_compact]) * old_edge;

			if (m_order.is_inner__orig(v1)) new_inner_diffs[m_order.to_new(v1)] += value;
			if (m_order.is_inner__orig(v2)) new_inner_diffs[m_order.to_new(v2)] -= value;
		}

		return new_inner_diffs;
	}

	Vectors RigidDeformSystem::solve_for_new_inner_positions(
		const std::array<Eigen::VectorXd, 3> &b_preprocessed,
		const Vectors &new_inner_diffs)
	{
		Vectors new_inner_positions(m_order.inner_amount());
		for (uint coord = 0; coord < 3; coord++) {
			Eigen::VectorXd b = new_inner_diffs.get_coord(coord) - b_preprocessed[coord];
#if USE_CHOLUP
			CholUp::Matrix<double> rhs(m_order.inner_amount(), 1);
			for (uint i = 0; i < m_order.inner_amount(); i++) rhs(i, 0) = b[i];

			m_solver_current.solve(rhs);

			Eigen::VectorXd result(m_order.inner_amount());
			for (uint i = 0; i < m_order.inner_amount(); i++) result[i] = rhs(i, 0);
#else
			Eigen::VectorXd result = m_solver->solve(b);
#endif
			new_inner_positions.set_coord(coord, result);
		}
		return new_inner_positions;
	}


	/* Reorder Data
	 *******************************************/

	/* expects the anchor indices to be sorted
	 * (6, [1, 4]) -> [0, 2, 3, 5,  1, 4] */
	static std::vector<uint> sort_vertices_by_anchors(const std::vector<uint> &anchors, uint vertex_amount)
	{
		std::vector<uint> sorted;

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

	ReorderData::ReorderData(const std::vector<uint> &anchors, uint vertex_amount)
	{
		m_new_to_orig = sort_vertices_by_anchors(anchors, vertex_amount);

		m_orig_to_new.resize(vertex_amount);
		for (int i = 0; i < vertex_amount; i++) {
			m_orig_to_new[m_new_to_orig[i]] = i;
		}

		this->m_inner_amount = vertex_amount - anchors.size();
	}


	/* Accessors
	***************************************/

	const std::vector<uint> &RigidDeformSystem::anchor_indices() const
	{
		return this->m_anchor_indices;
	}

	const std::vector<uint> &RigidDeformSystem::inner_indices() const
	{
		return this->m_inner_indices;
	}

	uint RigidDeformSystem::vertex_amount() const
	{
		return m_initial_positions.size();
	}

} /* namespace RigidDeform */