#pragma once

#include <iostream>
#include <memory>

#include "Eigen/Sparse"
#include "Eigen/Dense"

#define USE_CHOLUP 1

#if USE_CHOLUP
#    include "CholUp/CholUp.hpp"
#endif


namespace RigidDeform {

	typedef Eigen::Map<Eigen::VectorXd, 0, Eigen::InnerStride<3>> StridedVector;
	typedef Eigen::Map<const Eigen::VectorXd, 0, Eigen::InnerStride<3>> ConstStridedVector;
	typedef Eigen::SparseMatrix<double, Eigen::ColMajor> SparseMatrixD;
	typedef Eigen::Triplet<double> Triplet;
	typedef std::vector<Triplet> Triplets;

#if USE_CHOLUP
	typedef CholUp::SupernodalCholesky<CholUp::SparseMatrix<double>> Solver;
#else
	typedef Eigen::SimplicialLDLT<SparseMatrixD> Solver;
#endif

	class Vectors {
		Eigen::VectorXd m_data;

	public:
		Vectors() {}

		Vectors(int size)
		{
			m_data = Eigen::VectorXd(size * 3);
		}

		Vectors(std::vector<Eigen::Vector3d> &vectors)
		{
			m_data = Eigen::VectorXd(3 * vectors.size());
			memcpy(&m_data[0], &vectors[0][0], this->byte_size());
		}

		void set_zero()
		{
			m_data.setZero();
		}

		StridedVector get_coord(uint coord)
		{
			return StridedVector(m_data.data() + coord, this->size());
		}

		ConstStridedVector get_coord(uint coord) const
		{
			return ConstStridedVector(m_data.data() + coord, this->size());
		}

		void set_coord(uint coord, Eigen::VectorXd &values)
		{
			assert(values.size() == this->size());
			this->get_coord(coord) = values;
		}

		double *get_vector_ptr(uint index) const
		{
			return this->ptr() + (3 * index);
		}

		void set_vector_ptr(uint index, double *vector)
		{
			memcpy(this->ptr() + (3 * index), vector, sizeof(double) * 3);
		}

		Eigen::Map<Eigen::Vector3d> operator [](uint index) const
		{
			return Eigen::Map<Eigen::Vector3d>(this->get_vector_ptr(index));
		}

		double *ptr() const
		{
			return (double *)m_data.data();
		}

		uint size() const
		{
			return m_data.size() / 3;
		}

		uint byte_size() const
		{
			return this->size() * 3 * sizeof(double);
		}

		void print(std::string name) const
		{
			std::cout << name << ":" << std::endl;
			for (uint i = 0; i < this->size(); i++) {
				double *vector = this->get_vector_ptr(i);
				printf("  %7.3f %7.3f %7.3f\n", vector[0], vector[1], vector[2]);
			}
		}
	};

	struct ReorderData
	{
	private:
		std::vector<uint> m_orig_to_new;
		std::vector<uint> m_new_to_orig;
		uint m_inner_amount;

	public:
		ReorderData() = default;
		ReorderData(const std::vector<uint> &anchors, uint vertex_amount);

		uint inner_amount() const
		{
			return m_inner_amount;
		}

		uint anchor_amount() const
		{
			return m_orig_to_new.size() - this->inner_amount();
		}

		bool is_inner__orig(uint index) const
		{
			return m_orig_to_new[index] < m_inner_amount;
		}

		bool is_inner__new(uint index) const
		{
			return index < m_inner_amount;
		}

		uint to_orig(uint index) const
		{
			return m_new_to_orig[index];
		}

		uint to_new(uint index) const
		{
			return m_orig_to_new[index];
		}

		uint to_new_anchor(uint index) const
		{
			return this->to_new(index) - this->inner_amount();
		}
	};

	struct WeightedEdge {
		uint v1, v2;
		double weight;

		WeightedEdge() = default;

		WeightedEdge(uint v1, uint v2, double weight)
			: v1(v1), v2(v2), weight(weight) {}
	};

	typedef std::vector<WeightedEdge> WeightedEdges;

	struct ImpactData {
		WeightedEdges m_edges;
		std::vector<int> m_compact_map;
		uint m_compact_amount;

		uint compact_amount() const
		{
			return m_compact_amount;
		}

		const WeightedEdges &edges()
		{
			return m_edges;
		}

		int compact_index(uint index) const
		{
			return m_compact_map[index];
		}

	};

	class RigidDeformSystem {
	public:
		RigidDeformSystem(
			const Vectors &initial_positions,
			const std::vector<std::array<uint, 3>> &triangles);

		void set_anchors(
			const std::vector<uint> &anchor_indices);

		Vectors calculate_inner(
			const Vectors &anchor_positions,
			uint iterations);

		const std::vector<uint> &anchor_indices() const;
		const std::vector<uint> &inner_indices() const;

		uint vertex_amount() const;

	private:
		void update_inner_indices();
		void update_matrix();
		void update_impact_data();
		void get_impact_edges(
			WeightedEdges &r_impact_edges,
			std::vector<bool> &r_vertex_has_impact);

		std::vector<Eigen::Matrix3d> optimize_rotations(
			const Vectors &anchor_positions,
			const Vectors &new_inner_positions);

		Vectors optimize_inner_positions(
			const std::array<Eigen::VectorXd, 3> &b_preprocessed,
			const std::vector<Eigen::Matrix3d> &rotations);

		Vectors calculate_new_inner_diffs(
			const std::vector<Eigen::Matrix3d> &rotations);

		Vectors solve_for_new_inner_positions(
			const std::array<Eigen::VectorXd, 3> &b_preprocessed,
			const Vectors &new_inner_diffs);

	private:
		/* initialized once */
		Vectors m_initial_positions;
		WeightedEdges m_edges;
		Triplets m_laplace_triplets;

		/* updated for new anchor indices */
		std::vector<uint> m_anchor_indices;
		std::vector<uint> m_inner_indices;
		ReorderData m_order;
		ImpactData m_impact;
		SparseMatrixD m_A_II, m_A_IB;

#if USE_CHOLUP
		SparseMatrixD m_laplace_matrix;
		std::unique_ptr<Solver> m_solver;
		Solver m_solver_current;
#else
		std::unique_ptr<Solver> m_solver;
#endif
	};

 } /* namespace RigidDeform */