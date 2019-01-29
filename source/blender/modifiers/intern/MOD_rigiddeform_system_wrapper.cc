#include "MOD_rigiddeform_system.hpp"
#include "MOD_rigiddeform_system.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_mesh_runtime.h"

using namespace RigidDeform;

/* Utilities
 ****************************************/

static RigidDeformSystemRef wrap(RigidDeformSystem *system)
{
	return (RigidDeformSystemRef)system;
}

static RigidDeformSystem *unwrap(RigidDeformSystemRef system)
{
	return (RigidDeformSystem *)system;
}

static Vectors get_vertex_positions(Mesh *mesh)
{
	std::vector<Eigen::Vector3d> positions;

	for (uint i = 0; i < mesh->totvert; i++) {
		positions.push_back(Eigen::Vector3f(mesh->mvert[i].co).cast<double>());
	}

	return positions;
}

static std::vector<std::array<uint, 3>> get_triangle_indices(Mesh *mesh)
{
	std::vector<std::array<uint, 3>> indices;

	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

	for (uint i = 0; i < triangle_amount; i++) {
		uint v1 = mesh->mloop[triangles[i].tri[0]].v;
		uint v2 = mesh->mloop[triangles[i].tri[1]].v;
		uint v3 = mesh->mloop[triangles[i].tri[2]].v;
		indices.push_back({v1, v2, v3});
	}

	return indices;
}


/* Interface
 ***********************************/

RigidDeformSystemRef RigidDeformSystem_from_mesh(
        Mesh *mesh)
{
	return wrap(new RigidDeformSystem(
		get_vertex_positions(mesh),
		get_triangle_indices(mesh)));
}

void RigidDeformSystem_set_anchors(
        RigidDeformSystemRef system,
        uint *anchor_indices,
        uint anchor_amount)
{
	std::vector<uint> anchors_cpp(anchor_indices, anchor_indices + anchor_amount);
	unwrap(system)->set_anchors(anchors_cpp);
}

void RigidDeformSystem_correct_inner(
        RigidDeformSystemRef system,
        Vector3Ds positions,
        uint iterations)
{
	if (iterations == 0) {
		return;
	}

	RigidDeformSystem *system_cpp = unwrap(system);

	std::vector<Eigen::Vector3d> anchors;
	for (uint index : system_cpp->anchor_indices()) {
		anchors.push_back(Eigen::Vector3f(&positions[index][0]).cast<double>());
	}

	Vectors inner = system_cpp->calculate_inner(anchors, iterations);

	const std::vector<uint> &inner_indices = system_cpp->inner_indices();
	for (uint i = 0; i < inner_indices.size(); i++) {
		Eigen::Vector3f vector = inner[i].cast<float>();
		memcpy(&positions[inner_indices[i]][0], vector.data(), sizeof(float) * 3);
	}
}

void RigidDeformSystem_free(
        RigidDeformSystemRef system)
{
	delete unwrap(system);
}