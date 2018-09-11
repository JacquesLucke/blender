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
 * Contributor(s): Alexander Pinzon Fernandez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_laplaciandeform.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_particle.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MOD_util.h"

#include "eigen_capi.h"

typedef LaplacianDeformModifierBindData BindData;

typedef struct {
	SparseLeastSquaresSystemF *system;
} Cache;

static Cache *newCache()
{
	Cache *cache = MEM_mallocN(sizeof(Cache), __func__);
	cache->system = NULL;
	return cache;
}

static Cache *copyCache(Cache *source)
{
	Cache *cache = MEM_mallocN(sizeof(Cache), __func__);
	cache->system = NULL;
	return cache;
}

static void freeCacheContent(Cache *cache)
{
	if (cache->system != NULL) {
		EIG_SparseLeastSquaresSystemF_Delete(cache->system);
	}
}

static void freeCache(Cache *cache)
{
	freeCacheContent(cache);
	MEM_freeN(cache);
}

static bool vertexGroupExists(Object *ob, Mesh *mesh, const char *group_name)
{
	MDeformVert *dvert = NULL;
	int group_index = -1;
	MOD_get_vgroup(ob, mesh, group_name, &dvert, &group_index);
	return group_index >= 0 && dvert != NULL;
}

static void getAllVertexWeights(Object *ob, Mesh *mesh, const char *group_name, float *dst)
{
	MDeformVert *vertices;
	int group_index;

	MOD_get_vgroup(ob, mesh, group_name, &vertices, &group_index);

	for (int i = 0; i < mesh->totvert; i++) {
		dst[i] = defvert_find_weight(vertices + i, group_index);
	}
}

typedef struct {
	int *indices;
	int *starts;
} NeighboursMap;

static void freeNeighboursMap(NeighboursMap *map)
{
	MEM_freeN(map->starts);
	MEM_freeN(map->indices);
}

static void countDegreeOfEveryVertex(MEdge *edges, int edge_amount, int *degrees)
{
	for (int i = 0; i < edge_amount; i++) {
		MEdge *edge = edges + i;
		degrees[edge->v1]++;
		degrees[edge->v2]++;
	}
}

static void accumulateIntArray(int *array, int *target, int length)
{
	if (length == 0) return;

	target[0] = array[0];
	for (int i = 1; i < length; i++) {
		target[i] = target[i-1] + array[i];
	}
}

static int *calcNeighbourMapStarts(MEdge *edges, int vertex_amount, int edge_amount)
{
	int *starts = MEM_calloc_arrayN(vertex_amount + 1, sizeof(int), __func__);
	int *degrees = MEM_calloc_arrayN(vertex_amount, sizeof(int), __func__);

	countDegreeOfEveryVertex(edges, edge_amount, degrees);
	accumulateIntArray(degrees, starts + 1, vertex_amount);
	starts[0] = 0;

	MEM_freeN(degrees);

	return starts;
}

static int *calcNeighbourMapIndices(MEdge *edges, int *starts, int vertex_amount, int edge_amount)
{
	int *indices = MEM_malloc_arrayN(edge_amount * 2, sizeof(int), __func__);
	int *used_slots = MEM_calloc_arrayN(vertex_amount, sizeof(int), __func__);

	for (int i = 0; i < edge_amount; i++) {
		MEdge *edge = edges + i;
		BLI_assert(edge->v1 != edge->v2);

		indices[starts[edge->v1] + used_slots[edge->v1]] = edge->v2;
		indices[starts[edge->v2] + used_slots[edge->v2]] = edge->v1;
		used_slots[edge->v1]++;
		used_slots[edge->v2]++;
	}

	MEM_freeN(used_slots);

	return indices;
}

static NeighboursMap getNeighbourVerticesMap(MEdge *edges, int vertex_amount, int edge_amount)
{
	NeighboursMap map;
	map.starts = calcNeighbourMapStarts(edges, vertex_amount, edge_amount);
	map.indices = calcNeighbourMapIndices(edges, map.starts, vertex_amount, edge_amount);
	return map;
}

static void computeDifferentialCoordinates(
		NeighboursMap map, int vertex_amount,
		float (*vertices)[3], float (*result)[3])
{
	memset(result, 0, sizeof(float) * 3 * vertex_amount);
	for (int i = 0; i < vertex_amount; i++) {

		float weight_sum = 0.0f;
		for (int j = map.starts[i]; j < map.starts[i+1]; j++) {
			int neighbour = map.indices[j];
			float weight = 1;
			for (int coord = 0; coord < 3; coord++) {
				result[i][coord] -= weight * vertices[neighbour][coord];
			}
			weight_sum += weight;
		}

		for (int coord = 0; coord < 3; coord++) {
			if (weight_sum != 0) result[i][coord] /= weight_sum;
			result[i][coord] += vertices[i][coord];
		}
	}
}

typedef struct {
	int v1, v2;
	float weight;
} WeightedEdge;

static void calcWeightedEdgesFromTriangles(
		const MLoopTri *triangles, int triangle_amount,
		const MLoop *loops, const float (*vertices)[3], WeightedEdge *dst)
{
	for (int i = 0; i < triangle_amount; i++) {
		const MLoopTri *triangle = triangles + i;
		int v1 = loops[triangle->tri[0]].v;
		int v2 = loops[triangle->tri[1]].v;
		int v3 = loops[triangle->tri[2]].v;

		dst[i*3+0].v1 = v1;
		dst[i*3+0].v2 = v2;
		dst[i*3+0].weight = 1;

		dst[i*3+1].v1 = v2;
		dst[i*3+1].v2 = v3;
		dst[i*3+1].weight = 1;

		dst[i*3+2].v1 = v3;
		dst[i*3+2].v2 = v1;
		dst[i*3+2].weight = 1;
	}
}

static void calcTotalWeightPerVertex(WeightedEdge *edges, int edge_amount, float *dst, int vertex_amount)
{
	memset(dst, 0, sizeof(float) * vertex_amount);
	for (int i = 0; i < edge_amount; i++) {
		WeightedEdge edge = edges[i];
		dst[edge.v1] += edge.weight;
		dst[edge.v2] += edge.weight;
	}
}

static void insertLaplacianEntries(MatrixFEntries *entries, WeightedEdge *edges, int edge_amount, int vertex_amount)
{
	for (int i = 0; i < vertex_amount * 3; i++) {
		EIG_MatrixFEntries_Add(entries, i, i, 1);
	}

	float *total_weigths = MEM_malloc_arrayN(vertex_amount, sizeof(float), __func__);
	calcTotalWeightPerVertex(edges, edge_amount, total_weigths, vertex_amount);

	for (int i = 0; i < edge_amount; i++) {
		WeightedEdge edge = edges[i];

		if (edge.weight == 0) continue;
		BLI_assert(total_weigths[edge.v1] != 0);

		for (int coord = 0; coord < 3; coord++) {
			EIG_MatrixFEntries_Add(entries,
			        edge.v1 * 3 + coord,
			        edge.v2 * 3 + coord,
			        -edge.weight / total_weigths[edge.v1]);
			EIG_MatrixFEntries_Add(entries,
			        edge.v2 * 3 + coord,
			        edge.v1 * 3 + coord,
			        -edge.weight / total_weigths[edge.v2]);
		}
	}

	MEM_freeN(total_weigths);
}

static void getVertexPositions(Mesh *mesh, float (*dst)[3])
{
	for (int i = 0; i < mesh->totvert; i++) {
		copy_v3_v3((float*)(dst + i), mesh->mvert[i].co);
	}
}

static int countNonZeroIndices(float *values, int length)
{
	int amount = 0;
	for (int i = 0; i < length; i++) {
		if (values[i] != 0) amount++;
	}
	return amount;
}

static void getNonZeroIndices(float *values, int length, int **indices_dst, int *amount_dst)
{
	int amount = countNonZeroIndices(values, length);
	int *indices = MEM_malloc_arrayN(amount, sizeof(int), __func__);

	int index = 0;
	for (int i = 0; i < length; i++) {
		if (values[i] != 0) {
			indices[index] = i;
			index++;
		}
	}

	*indices_dst = indices;
	*amount_dst = amount;
}

static void getNonZeroWeightIndices(
        Object *object, Mesh *mesh, const char *weight_group_name,
        int **indices_dst, int *amount_dst)
{
	int vertex_amount = mesh->totvert;
	float *weights = MEM_malloc_arrayN(vertex_amount, sizeof(float), __func__);
	getAllVertexWeights(object, mesh, weight_group_name, weights);
	getNonZeroIndices(weights, vertex_amount, indices_dst, amount_dst);
}

static void getAnchorIndices(
        Object *object, Mesh *mesh, const char *anchor_group_name,
        int **indices_dst, int *amount_dst)
{
	getNonZeroWeightIndices(object, mesh, anchor_group_name, indices_dst, amount_dst);
}

static bool hasBindData(LaplacianDeformModifierData *modifier)
{
	return modifier->bind_data != NULL;
}

static bool hasCache(LaplacianDeformModifierData *modifier)
{
	return modifier->cache != NULL;
}

static void removeCacheFromModifierIfExistent(LaplacianDeformModifierData *modifier)
{
	if (hasCache(modifier)) {
		freeCache(modifier->cache);
		modifier->cache = NULL;
	}
}

static void freeBindDataContent(BindData *bind_data)
{
	MEM_freeN(bind_data->anchor_indices);
	MEM_freeN(bind_data->vertex_positions);
}

static void freeBindData(BindData *bind_data)
{
	freeBindDataContent(bind_data);
	MEM_freeN(bind_data);
}

static void removeBindDataFromModifier(LaplacianDeformModifierData *modifier)
{
	freeBindData(modifier->bind_data);
	modifier->bind_data = NULL;
}

static void removeBindDataFromModifierIfExistent(LaplacianDeformModifierData *modifier)
{
	if (hasBindData(modifier)) {
		removeBindDataFromModifier(modifier);
	}
}

static BindData *copyBindData(BindData *source)
{
	BindData *data;
	data = MEM_mallocN(sizeof(BindData), __func__);

	data->anchor_indices = MEM_malloc_arrayN(source->anchor_amount, sizeof(int), __func__);
	data->vertex_positions = MEM_malloc_arrayN(source->vertex_amount, sizeof(float) * 3, __func__);

	memcpy(data->anchor_indices, source->anchor_indices, sizeof(int) * source->anchor_amount);
	memcpy(data->vertex_positions, source->vertex_positions, sizeof(float) * 3 * source->vertex_amount);

	data->anchor_amount = source->anchor_amount;
	data->vertex_amount = source->vertex_amount;

	return data;
}

static BindData *newBindData(Object *object, Mesh *mesh, LaplacianDeformModifierData *modifier)
{
	if (!vertexGroupExists(object, mesh, modifier->anchor_group_name)) {
		return NULL;
	}

	BindData *data;
	data = MEM_mallocN(sizeof(BindData), __func__);

	getAnchorIndices(object, mesh, modifier->anchor_group_name,
		&data->anchor_indices, &data->anchor_amount);

	int vertex_amount = mesh->totvert;
	data->vertex_amount = vertex_amount;
	data->vertex_positions = MEM_malloc_arrayN(vertex_amount, sizeof(float) * 3, __func__);
	getVertexPositions(mesh, data->vertex_positions);

	return data;
}

void MOD_LaplacianDeform_Unbind(LaplacianDeformModifierData *modifier)
{
	removeBindDataFromModifierIfExistent(modifier);
	removeCacheFromModifierIfExistent(modifier);
}

int MOD_LaplacianDeform_Bind(Object *object, Mesh *mesh, LaplacianDeformModifierData *modifier)
{
	removeBindDataFromModifierIfExistent(modifier);
	modifier->bind_data = newBindData(object, mesh, modifier);

	if (modifier->bind_data == NULL) return -1;
	else return 0;
}

static bool bindDataIsValid(BindData *data, Object *object, Mesh *mesh)
{
	return data->vertex_amount == mesh->totvert;
}

static Cache *getCache(LaplacianDeformModifierData *modifier)
{
	Cache *cache = modifier->cache;
	BLI_assert(cache);
	return cache;
}

static SparseLeastSquaresSystemF *getCachedSystem(LaplacianDeformModifierData *modifier)
{
	SparseLeastSquaresSystemF *system = getCache(modifier)->system;
	BLI_assert(system);
	return system;
}

static bool hasCachedSystem(LaplacianDeformModifierData *modifier)
{
	return hasCache(modifier) && getCache(modifier)->system != NULL;
}

static void freeSystemIfExistent(LaplacianDeformModifierData *modifier)
{
	if (hasCachedSystem(modifier)) {
		Cache *cache = getCache(modifier);
		EIG_SparseLeastSquaresSystemF_Delete(cache->system);
		cache->system = NULL;
	}
}


static void cacheSystem(Cache *cache, SparseLeastSquaresSystemF *system)
{
	cache->system = system;
}

static void insertAnchorEntries(MatrixFEntries *entries, int *anchor_indices, int anchor_amount, int row_offset)
{
	for (int i = 0; i < anchor_amount; i++) {
		int anchor = anchor_indices[i];
		for (int coord = 0; coord < 3; coord++) {
			int row = row_offset + i * 3 + coord;
			int col = anchor * 3 + coord;
			EIG_MatrixFEntries_Add(entries, row, col, 1);
		}
	}
}

static void fillSystemMatrix(MatrixFEntries *entries, int *rows, int *cols,
        LaplacianDeformModifierData *modifier, Object *object, Mesh *mesh, float (*vertex_positions)[3])
{
	int vertex_amount = mesh->totvert;
	BindData *bind_data = modifier->bind_data;
	*rows = vertex_amount * 3 + bind_data->anchor_amount * 3;
	*cols = vertex_amount * 3;

	const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
	int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);
	int edge_amount = triangle_amount * 3;

	WeightedEdge *weigthed_edges = MEM_malloc_arrayN(edge_amount, sizeof(WeightedEdge), __func__);
	calcWeightedEdgesFromTriangles(triangles, triangle_amount, mesh->mloop, vertex_positions, weigthed_edges);
	insertLaplacianEntries(entries, weigthed_edges, edge_amount, vertex_amount);
	MEM_freeN(weigthed_edges);

	insertAnchorEntries(entries, bind_data->anchor_indices, bind_data->anchor_amount, vertex_amount * 3);
}

static SparseMatrixF *constructSystemMatrix(
        LaplacianDeformModifierData *modifier, Object *object, Mesh *mesh,
        float (*vertex_positions)[3])
{
	MatrixFEntries *entries = EIG_MatrixFEntries_New();
	int rows, cols;
	fillSystemMatrix(entries, &rows, &cols,
	    modifier, object, mesh, vertex_positions);
	SparseMatrixF *matrix = EIG_SparseMatrixF_FromEntries(rows, cols, entries);
	EIG_MatrixFEntries_Delete(entries);
	return matrix;
}

static SparseLeastSquaresSystemF *calculateSystem(
        LaplacianDeformModifierData *modifier, Object *object, Mesh *mesh,
        float (*vertex_positions)[3])
{
	SparseMatrixF *systemMatrix = constructSystemMatrix(modifier, object, mesh, vertex_positions);
	EIG_SparseMatrixF_Print(systemMatrix);
	SparseLeastSquaresSystemF *system = EIG_SparseLeastSquaresSystemF_FromSystemMatrix(systemMatrix);
	EIG_SparseMatrixF_Delete(systemMatrix);
	return system;
}

static SparseLeastSquaresSystemF *getSystem(
        LaplacianDeformModifierData *modifier, BindData *bind_data,
        Object *object, Mesh *mesh, float (*vertex_positions)[3])
{
	if (!hasCachedSystem(modifier)) {
		SparseLeastSquaresSystemF *system;
		system = calculateSystem(modifier, object, mesh, vertex_positions);
		cacheSystem(getCache(modifier), system);
	}
	return getCachedSystem(modifier);
}

static void ensureCacheExists(LaplacianDeformModifierData *modifier)
{
	if (!hasCache(modifier)) {
		modifier->cache = newCache();
	}
}

static BindData *getBindData(LaplacianDeformModifierData *modifier)
{
	BindData *data = modifier->bind_data;
	BLI_assert(data);
	return data;
}

static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *modifier, Object *object, Mesh *mesh,
        float (*vertex_positions)[3], int vertex_amount)
{
	if (!hasBindData(modifier)) {
		return;
	}

	BindData *bind_data = getBindData(modifier);
	ensureCacheExists(modifier);

	if (!bindDataIsValid(bind_data, object, mesh)) {
		modifier_setError(&modifier->modifier, "bind data is not valid anymore");
		return;
	}

	SparseLeastSquaresSystemF *system = getSystem(modifier, bind_data, object, mesh, vertex_positions);

	float *result = MEM_malloc_arrayN(vertex_amount, sizeof(float) * 3, __func__);
	float *b = MEM_malloc_arrayN(vertex_amount * 3 + modifier->bind_data->anchor_amount * 3, sizeof(float), __func__);
}

static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *modifier = (LaplacianDeformModifierData *)md;
	modifier->anchor_group_name[0] = 0;
	modifier->bind_data = NULL;
	modifier->cache = NULL;
	modifier->is_main = true;
}

static void copyData(const ModifierData *_source, ModifierData *_target, const int flag)
{
	const LaplacianDeformModifierData *source = (const LaplacianDeformModifierData *)_source;
	LaplacianDeformModifierData *target = (LaplacianDeformModifierData *)_target;

	modifier_copyData_generic(_source, _target, flag);

	if (flag & LIB_ID_CREATE_NO_MAIN) {
		target->bind_data = source->bind_data;
		target->cache = source->cache;
		target->is_main = false;
	} else {
		target->bind_data = copyBindData(source->bind_data);
		target->cache = copyCache(source->cache);
		target->is_main = true;
	}
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	return 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ctx->object, mesh_src, vertexCos, numVerts);
	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, editData, mesh, NULL, false, false);
	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ctx->object, mesh_src,
	                           vertexCos, numVerts);
	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void freeData(ModifierData *md)
{
	LaplacianDeformModifierData *modifier = (LaplacianDeformModifierData *)md;
	if (modifier->is_main) {
		MOD_LaplacianDeform_Unbind(modifier);
	}
}

ModifierTypeInfo modifierType_LaplacianDeform = {
	/* name */              "LaplacianDeform",
	/* structName */        "LaplacianDeformModifierData",
	/* structSize */        sizeof(LaplacianDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};