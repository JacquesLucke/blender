/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include <algorithm>
#include <atomic>
#include <numeric>

#include "BLI_devirtualize_parameters.hh"
#include "BLI_kdtree.h"
#include "BLI_rand.hh"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "ED_curves.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "WM_api.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "GEO_reverse_uv_sampler.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 * `ha`: Local space of an individual hair in the legacy hair system.
 */

namespace blender::ed::curves {

static bool object_has_editable_curves(const Main &bmain, const Object &object)
{
  if (object.type != OB_CURVES) {
    return false;
  }
  if (!ELEM(object.mode, OB_MODE_SCULPT_CURVES, OB_MODE_EDIT)) {
    return false;
  }
  if (!BKE_id_is_editable(&bmain, static_cast<const ID *>(object.data))) {
    return false;
  }
  return true;
}

static VectorSet<Curves *> get_unique_editable_curves(const bContext &C)
{
  VectorSet<Curves *> unique_curves;

  const Main &bmain = *CTX_data_main(&C);

  Object *object = CTX_data_active_object(&C);
  if (object && object_has_editable_curves(bmain, *object)) {
    unique_curves.add_new(static_cast<Curves *>(object->data));
  }

  CTX_DATA_BEGIN (&C, Object *, object, selected_objects) {
    if (object_has_editable_curves(bmain, *object)) {
      unique_curves.add(static_cast<Curves *>(object->data));
    }
  }
  CTX_DATA_END;

  return unique_curves;
}

using bke::CurvesGeometry;

namespace convert_to_particle_system {

static int find_mface_for_root_position(const Mesh &mesh,
                                        const Span<int> possible_mface_indices,
                                        const float3 &root_pos)
{
  BLI_assert(possible_mface_indices.size() >= 1);
  if (possible_mface_indices.size() == 1) {
    return possible_mface_indices.first();
  }
  /* Find the closest #MFace to #root_pos. */
  int mface_i;
  float best_distance_sq = FLT_MAX;
  for (const int possible_mface_i : possible_mface_indices) {
    const MFace &possible_mface = mesh.mface[possible_mface_i];
    {
      float3 point_in_triangle;
      closest_on_tri_to_point_v3(point_in_triangle,
                                 root_pos,
                                 mesh.mvert[possible_mface.v1].co,
                                 mesh.mvert[possible_mface.v2].co,
                                 mesh.mvert[possible_mface.v3].co);
      const float distance_sq = len_squared_v3v3(root_pos, point_in_triangle);
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        mface_i = possible_mface_i;
      }
    }
    /* Optionally check the second triangle if the #MFace is a quad. */
    if (possible_mface.v4) {
      float3 point_in_triangle;
      closest_on_tri_to_point_v3(point_in_triangle,
                                 root_pos,
                                 mesh.mvert[possible_mface.v1].co,
                                 mesh.mvert[possible_mface.v3].co,
                                 mesh.mvert[possible_mface.v4].co);
      const float distance_sq = len_squared_v3v3(root_pos, point_in_triangle);
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        mface_i = possible_mface_i;
      }
    }
  }
  return mface_i;
}

/**
 * \return Barycentric coordinates in the #MFace.
 */
static float4 compute_mface_weights_for_position(const Mesh &mesh,
                                                 const MFace &mface,
                                                 const float3 &position)
{
  float4 mface_weights;
  if (mface.v4) {
    float mface_verts_su[4][3];
    copy_v3_v3(mface_verts_su[0], mesh.mvert[mface.v1].co);
    copy_v3_v3(mface_verts_su[1], mesh.mvert[mface.v2].co);
    copy_v3_v3(mface_verts_su[2], mesh.mvert[mface.v3].co);
    copy_v3_v3(mface_verts_su[3], mesh.mvert[mface.v4].co);
    interp_weights_poly_v3(mface_weights, mface_verts_su, 4, position);
  }
  else {
    interp_weights_tri_v3(mface_weights,
                          mesh.mvert[mface.v1].co,
                          mesh.mvert[mface.v2].co,
                          mesh.mvert[mface.v3].co,
                          position);
    mface_weights[3] = 0.0f;
  }
  return mface_weights;
}

static void try_convert_single_object(Object &curves_ob,
                                      Main &bmain,
                                      Scene &scene,
                                      bool *r_could_not_convert_some_curves)
{
  if (curves_ob.type != OB_CURVES) {
    return;
  }
  Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  if (curves_id.surface == nullptr) {
    return;
  }
  Object &surface_ob = *curves_id.surface;
  if (surface_ob.type != OB_MESH) {
    return;
  }
  Mesh &surface_me = *static_cast<Mesh *>(surface_ob.data);

  BVHTreeFromMesh surface_bvh;
  BKE_bvhtree_from_mesh_get(&surface_bvh, &surface_me, BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

  const Span<float3> positions_cu = curves.positions();
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&surface_me),
                                BKE_mesh_runtime_looptri_len(&surface_me)};

  if (looptris.is_empty()) {
    *r_could_not_convert_some_curves = true;
  }

  const int hair_num = curves.curves_num();
  if (hair_num == 0) {
    return;
  }

  ParticleSystem *particle_system = nullptr;
  LISTBASE_FOREACH (ParticleSystem *, psys, &surface_ob.particlesystem) {
    if (STREQ(psys->name, curves_ob.id.name + 2)) {
      particle_system = psys;
      break;
    }
  }
  if (particle_system == nullptr) {
    ParticleSystemModifierData &psmd = *reinterpret_cast<ParticleSystemModifierData *>(
        object_add_particle_system(&bmain, &scene, &surface_ob, curves_ob.id.name + 2));
    particle_system = psmd.psys;
    particle_system->part->draw_step = 3;
  }

  ParticleSettings &settings = *particle_system->part;

  psys_free_particles(particle_system);
  settings.type = PART_HAIR;
  settings.totpart = 0;
  psys_changed_type(&surface_ob, particle_system);

  MutableSpan<ParticleData> particles{
      static_cast<ParticleData *>(MEM_calloc_arrayN(hair_num, sizeof(ParticleData), __func__)),
      hair_num};

  /* The old hair system still uses #MFace, so make sure those are available on the mesh. */
  BKE_mesh_tessface_calc(&surface_me);

  /* Prepare utility data structure to map hair roots to #MFace's. */
  const Span<int> mface_to_poly_map{
      static_cast<const int *>(CustomData_get_layer(&surface_me.fdata, CD_ORIGINDEX)),
      surface_me.totface};
  Array<Vector<int>> poly_to_mface_map(surface_me.totpoly);
  for (const int mface_i : mface_to_poly_map.index_range()) {
    const int poly_i = mface_to_poly_map[mface_i];
    poly_to_mface_map[poly_i].append(mface_i);
  }

  /* Prepare transformation matrices. */
  const float4x4 curves_to_world_mat = curves_ob.obmat;
  const float4x4 surface_to_world_mat = surface_ob.obmat;
  const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();
  const float4x4 curves_to_surface_mat = world_to_surface_mat * curves_to_world_mat;

  for (const int new_hair_i : IndexRange(hair_num)) {
    const int curve_i = new_hair_i;
    const IndexRange points = curves.points_for_curve(curve_i);

    const float3 &root_pos_cu = positions_cu[points.first()];
    const float3 root_pos_su = curves_to_surface_mat * root_pos_cu;

    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    BLI_bvhtree_find_nearest(
        surface_bvh.tree, root_pos_su, &nearest, surface_bvh.nearest_callback, &surface_bvh);
    BLI_assert(nearest.index >= 0);

    const int looptri_i = nearest.index;
    const MLoopTri &looptri = looptris[looptri_i];
    const int poly_i = looptri.poly;

    const int mface_i = find_mface_for_root_position(
        surface_me, poly_to_mface_map[poly_i], root_pos_su);
    const MFace &mface = surface_me.mface[mface_i];

    const float4 mface_weights = compute_mface_weights_for_position(
        surface_me, mface, root_pos_su);

    ParticleData &particle = particles[new_hair_i];
    const int num_keys = points.size();
    MutableSpan<HairKey> hair_keys{
        static_cast<HairKey *>(MEM_calloc_arrayN(num_keys, sizeof(HairKey), __func__)), num_keys};

    particle.hair = hair_keys.data();
    particle.totkey = hair_keys.size();
    copy_v4_v4(particle.fuv, mface_weights);
    particle.num = mface_i;
    /* Not sure if there is a better way to initialize this. */
    particle.num_dmcache = DMCACHE_NOTFOUND;

    float4x4 hair_to_surface_mat;
    psys_mat_hair_to_object(
        &surface_ob, &surface_me, PART_FROM_FACE, &particle, hair_to_surface_mat.values);
    /* In theory, #psys_mat_hair_to_object should handle this, but it doesn't right now. */
    copy_v3_v3(hair_to_surface_mat.values[3], root_pos_su);
    const float4x4 surface_to_hair_mat = hair_to_surface_mat.inverted();

    for (const int key_i : hair_keys.index_range()) {
      const float3 &key_pos_cu = positions_cu[points[key_i]];
      const float3 key_pos_su = curves_to_surface_mat * key_pos_cu;
      const float3 key_pos_ha = surface_to_hair_mat * key_pos_su;

      HairKey &key = hair_keys[key_i];
      copy_v3_v3(key.co, key_pos_ha);
      key.time = 100.0f * key_i / (float)(hair_keys.size() - 1);
    }
  }

  particle_system->particles = particles.data();
  particle_system->totpart = particles.size();
  particle_system->flag |= PSYS_EDITED;
  particle_system->recalc |= ID_RECALC_PSYS_RESET;

  DEG_id_tag_update(&surface_ob.id, ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&settings.id, ID_RECALC_COPY_ON_WRITE);
}

static int curves_convert_to_particle_system_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);

  bool could_not_convert_some_curves = false;

  Object &active_object = *CTX_data_active_object(C);
  try_convert_single_object(active_object, bmain, scene, &could_not_convert_some_curves);

  CTX_DATA_BEGIN (C, Object *, curves_ob, selected_objects) {
    if (curves_ob != &active_object) {
      try_convert_single_object(*curves_ob, bmain, scene, &could_not_convert_some_curves);
    }
  }
  CTX_DATA_END;

  if (could_not_convert_some_curves) {
    BKE_report(op->reports,
               RPT_INFO,
               "Some curves could not be converted because they were not attached to the surface");
  }

  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool curves_convert_to_particle_system_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_CURVES) {
    return false;
  }
  Curves &curves = *static_cast<Curves *>(ob->data);
  return curves.surface != nullptr;
}

}  // namespace convert_to_particle_system

static void CURVES_OT_convert_to_particle_system(wmOperatorType *ot)
{
  ot->name = "Convert Curves to Particle System";
  ot->idname = "CURVES_OT_convert_to_particle_system";
  ot->description = "Add a new or update an existing hair particle system on the surface object";

  ot->poll = convert_to_particle_system::curves_convert_to_particle_system_poll;
  ot->exec = convert_to_particle_system::curves_convert_to_particle_system_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

namespace convert_from_particle_system {

static bke::CurvesGeometry particles_to_curves(Object &object, ParticleSystem &psys)
{
  ParticleSettings &settings = *psys.part;
  if (psys.part->type != PART_HAIR) {
    return {};
  }

  const bool transfer_parents = (settings.draw & PART_DRAW_PARENT) || settings.childtype == 0;

  const Span<ParticleCacheKey *> parents_cache{psys.pathcache, psys.totcached};
  const Span<ParticleCacheKey *> children_cache{psys.childcache, psys.totchildcache};

  int points_num = 0;
  Vector<int> curve_offsets;
  Vector<int> parents_to_transfer;
  Vector<int> children_to_transfer;
  if (transfer_parents) {
    for (const int parent_i : parents_cache.index_range()) {
      const int segments = parents_cache[parent_i]->segments;
      if (segments <= 0) {
        continue;
      }
      parents_to_transfer.append(parent_i);
      curve_offsets.append(points_num);
      points_num += segments + 1;
    }
  }
  for (const int child_i : children_cache.index_range()) {
    const int segments = children_cache[child_i]->segments;
    if (segments <= 0) {
      continue;
    }
    children_to_transfer.append(child_i);
    curve_offsets.append(points_num);
    points_num += segments + 1;
  }
  const int curves_num = parents_to_transfer.size() + children_to_transfer.size();
  curve_offsets.append(points_num);
  BLI_assert(curve_offsets.size() == curves_num + 1);
  bke::CurvesGeometry curves(points_num, curves_num);
  curves.offsets_for_write().copy_from(curve_offsets);

  const float4x4 object_to_world_mat = object.obmat;
  const float4x4 world_to_object_mat = object_to_world_mat.inverted();

  MutableSpan<float3> positions = curves.positions_for_write();

  const auto copy_hair_to_curves = [&](const Span<ParticleCacheKey *> hair_cache,
                                       const Span<int> indices_to_transfer,
                                       const int curve_index_offset) {
    threading::parallel_for(indices_to_transfer.index_range(), 256, [&](const IndexRange range) {
      for (const int i : range) {
        const int hair_i = indices_to_transfer[i];
        const int curve_i = i + curve_index_offset;
        const IndexRange points = curves.points_for_curve(curve_i);
        const Span<ParticleCacheKey> keys{hair_cache[hair_i], points.size()};
        for (const int key_i : keys.index_range()) {
          const float3 key_pos_wo = keys[key_i].co;
          positions[points[key_i]] = world_to_object_mat * key_pos_wo;
        }
      }
    });
  };

  if (transfer_parents) {
    copy_hair_to_curves(parents_cache, parents_to_transfer, 0);
  }
  copy_hair_to_curves(children_cache, children_to_transfer, parents_to_transfer.size());

  curves.update_curve_types();
  curves.tag_topology_changed();
  return curves;
}

static int curves_convert_from_particle_system_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main &bmain = *CTX_data_main(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object *ob_from_orig = ED_object_active_context(C);
  ParticleSystem *psys_orig = static_cast<ParticleSystem *>(
      CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
  if (psys_orig == nullptr) {
    psys_orig = psys_get_current(ob_from_orig);
  }
  if (psys_orig == nullptr) {
    return OPERATOR_CANCELLED;
  }
  Object *ob_from_eval = DEG_get_evaluated_object(&depsgraph, ob_from_orig);
  ParticleSystem *psys_eval = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &ob_from_eval->modifiers) {
    if (md->type != eModifierType_ParticleSystem) {
      continue;
    }
    ParticleSystemModifierData *psmd = reinterpret_cast<ParticleSystemModifierData *>(md);
    if (!STREQ(psmd->psys->name, psys_orig->name)) {
      continue;
    }
    psys_eval = psmd->psys;
  }

  Object *ob_new = BKE_object_add(&bmain, &view_layer, OB_CURVES, psys_eval->name);
  ob_new->dtx |= OB_DRAWBOUNDOX; /* TODO: Remove once there is actual drawing. */
  Curves *curves_id = static_cast<Curves *>(ob_new->data);
  BKE_object_apply_mat4(ob_new, ob_from_orig->obmat, true, false);
  bke::CurvesGeometry::wrap(curves_id->geometry) = particles_to_curves(*ob_from_eval, *psys_eval);

  DEG_relations_tag_update(&bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

static bool curves_convert_from_particle_system_poll(bContext *C)
{
  return ED_object_active_context(C) != nullptr;
}

}  // namespace convert_from_particle_system

static void CURVES_OT_convert_from_particle_system(wmOperatorType *ot)
{
  ot->name = "Convert Particle System to Curves";
  ot->idname = "CURVES_OT_convert_from_particle_system";
  ot->description = "Add a new curves object based on the current state of the particle system";

  ot->poll = convert_from_particle_system::curves_convert_from_particle_system_poll;
  ot->exec = convert_from_particle_system::curves_convert_from_particle_system_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

namespace snap_curves_to_surface {

enum class AttachMode {
  Nearest,
  Deform,
};

static bool snap_curves_to_surface_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_CURVES) {
    return false;
  }
  if (!ED_operator_object_active_editable_ex(C, ob)) {
    return false;
  }
  Curves &curves = *static_cast<Curves *>(ob->data);
  if (curves.surface == nullptr) {
    return false;
  }
  return true;
}

static int snap_curves_to_surface_exec(bContext *C, wmOperator *op)
{
  const AttachMode attach_mode = static_cast<AttachMode>(RNA_enum_get(op->ptr, "attach_mode"));

  std::atomic<bool> found_invalid_uv = false;

  CTX_DATA_BEGIN (C, Object *, curves_ob, selected_objects) {
    if (curves_ob->type != OB_CURVES) {
      continue;
    }
    Curves &curves_id = *static_cast<Curves *>(curves_ob->data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    if (curves_id.surface == nullptr) {
      continue;
    }
    Object &surface_ob = *curves_id.surface;
    if (surface_ob.type != OB_MESH) {
      continue;
    }
    Mesh &surface_mesh = *static_cast<Mesh *>(surface_ob.data);

    MeshComponent surface_mesh_component;
    surface_mesh_component.replace(&surface_mesh, GeometryOwnershipType::ReadOnly);

    VArray_Span<float2> surface_uv_map;
    if (curves_id.surface_uv_map != nullptr) {
      surface_uv_map = surface_mesh_component
                           .attribute_try_get_for_read(
                               curves_id.surface_uv_map, ATTR_DOMAIN_CORNER, CD_PROP_FLOAT2)
                           .typed<float2>();
    }

    MutableSpan<float3> positions_cu = curves.positions_for_write();
    MutableSpan<float2> surface_uv_coords = curves.surface_uv_coords_for_write();

    const Span<MLoopTri> surface_looptris = {BKE_mesh_runtime_looptri_ensure(&surface_mesh),
                                             BKE_mesh_runtime_looptri_len(&surface_mesh)};

    const float4x4 curves_to_world_mat = curves_ob->obmat;
    const float4x4 world_to_curves_mat = curves_to_world_mat.inverted();
    const float4x4 surface_to_world_mat = surface_ob.obmat;
    const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();
    const float4x4 curves_to_surface_mat = world_to_surface_mat * curves_to_world_mat;
    const float4x4 surface_to_curves_mat = world_to_curves_mat * surface_to_world_mat;

    switch (attach_mode) {
      case AttachMode::Nearest: {
        BVHTreeFromMesh surface_bvh;
        BKE_bvhtree_from_mesh_get(&surface_bvh, &surface_mesh, BVHTREE_FROM_LOOPTRI, 2);
        BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

        threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
          for (const int curve_i : curves_range) {
            const IndexRange points = curves.points_for_curve(curve_i);
            const int first_point_i = points.first();
            const float3 old_first_point_pos_cu = positions_cu[first_point_i];
            const float3 old_first_point_pos_su = curves_to_surface_mat * old_first_point_pos_cu;

            BVHTreeNearest nearest;
            nearest.index = -1;
            nearest.dist_sq = FLT_MAX;
            BLI_bvhtree_find_nearest(surface_bvh.tree,
                                     old_first_point_pos_su,
                                     &nearest,
                                     surface_bvh.nearest_callback,
                                     &surface_bvh);
            const int looptri_index = nearest.index;
            if (looptri_index == -1) {
              continue;
            }

            const float3 new_first_point_pos_su = nearest.co;
            const float3 new_first_point_pos_cu = surface_to_curves_mat * new_first_point_pos_su;
            const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;

            for (float3 &pos_cu : positions_cu.slice(points)) {
              pos_cu += pos_diff_cu;
            }

            if (!surface_uv_map.is_empty()) {
              const MLoopTri &looptri = surface_looptris[looptri_index];
              const int corner0 = looptri.tri[0];
              const int corner1 = looptri.tri[1];
              const int corner2 = looptri.tri[2];
              const float2 &uv0 = surface_uv_map[corner0];
              const float2 &uv1 = surface_uv_map[corner1];
              const float2 &uv2 = surface_uv_map[corner2];
              const float3 &p0_su = surface_mesh.mvert[surface_mesh.mloop[corner0].v].co;
              const float3 &p1_su = surface_mesh.mvert[surface_mesh.mloop[corner1].v].co;
              const float3 &p2_su = surface_mesh.mvert[surface_mesh.mloop[corner2].v].co;
              float3 bary_coords;
              interp_weights_tri_v3(bary_coords, p0_su, p1_su, p2_su, new_first_point_pos_su);
              const float2 uv = attribute_math::mix3(bary_coords, uv0, uv1, uv2);
              surface_uv_coords[curve_i] = uv;
            }
          }
        });
        break;
      }
      case AttachMode::Deform: {
        if (surface_uv_map.is_empty()) {
          BKE_report(op->reports,
                     RPT_ERROR,
                     "Curves do not have attachment information that can be used for deformation");
          break;
        }
        using geometry::ReverseUVSampler;
        ReverseUVSampler reverse_uv_sampler{surface_uv_map, surface_looptris};

        threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
          for (const int curve_i : curves_range) {
            const IndexRange points = curves.points_for_curve(curve_i);
            const int first_point_i = points.first();
            const float3 old_first_point_pos_cu = positions_cu[first_point_i];

            const float2 uv = surface_uv_coords[curve_i];
            ReverseUVSampler::Result lookup_result = reverse_uv_sampler.sample(uv);
            if (lookup_result.type != ReverseUVSampler::ResultType::Ok) {
              found_invalid_uv = true;
              continue;
            }

            const MLoopTri &looptri = *lookup_result.looptri;
            const float3 &bary_coords = lookup_result.bary_weights;

            const float3 &p0_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[0]].v].co;
            const float3 &p1_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[1]].v].co;
            const float3 &p2_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[2]].v].co;

            float3 new_first_point_pos_su;
            interp_v3_v3v3v3(new_first_point_pos_su, p0_su, p1_su, p2_su, bary_coords);
            const float3 new_first_point_pos_cu = surface_to_curves_mat * new_first_point_pos_su;

            const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;
            for (float3 &pos_cu : positions_cu.slice(points)) {
              pos_cu += pos_diff_cu;
            }
          }
        });
        break;
      }
    }

    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
  }
  CTX_DATA_END;

  if (found_invalid_uv) {
    BKE_report(op->reports, RPT_INFO, "Could not snap some curves to the surface");
  }

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace snap_curves_to_surface

static void CURVES_OT_snap_curves_to_surface(wmOperatorType *ot)
{
  using namespace snap_curves_to_surface;

  ot->name = "Snap Curves to Surface";
  ot->idname = "CURVES_OT_snap_curves_to_surface";
  ot->description = "Move curves so that the first point is exactly on the surface mesh";

  ot->poll = snap_curves_to_surface_poll;
  ot->exec = snap_curves_to_surface_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  static const EnumPropertyItem attach_mode_items[] = {
      {static_cast<int>(AttachMode::Nearest),
       "NEAREST",
       0,
       "Nearest",
       "Find the closest point on the surface for the root point of every curve and move the root "
       "there"},
      {static_cast<int>(AttachMode::Deform),
       "DEFORM",
       0,
       "Deform",
       "Re-attach curves to a deformed surface using the existing attachment information. This "
       "only works when the topology of the surface mesh has not changed"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "attach_mode",
               attach_mode_items,
               static_cast<int>(AttachMode::Nearest),
               "Attach Mode",
               "How to find the point on the surface to attach to");
}

static bool selection_poll(bContext *C)
{
  const Object *object = CTX_data_active_object(C);
  if (object == nullptr) {
    return false;
  }
  if (object->type != OB_CURVES) {
    return false;
  }
  if (!BKE_id_is_editable(CTX_data_main(C), static_cast<const ID *>(object->data))) {
    return false;
  }
  return true;
}

namespace set_selection_domain {

static int curves_set_selection_domain_exec(bContext *C, wmOperator *op)
{
  const eAttrDomain domain = eAttrDomain(RNA_enum_get(op->ptr, "domain"));

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    if (curves_id->selection_domain == domain && (curves_id->flag & CV_SCULPT_SELECTION_ENABLED)) {
      continue;
    }

    const eAttrDomain old_domain = eAttrDomain(curves_id->selection_domain);
    curves_id->selection_domain = domain;
    curves_id->flag |= CV_SCULPT_SELECTION_ENABLED;

    CurveComponent component;
    component.replace(curves_id, GeometryOwnershipType::Editable);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);

    if (old_domain == ATTR_DOMAIN_POINT && domain == ATTR_DOMAIN_CURVE) {
      VArray<float> curve_selection = curves.adapt_domain(
          curves.selection_point_float(), ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE);
      curve_selection.materialize(curves.selection_curve_float_for_write());
      component.attribute_try_delete(".selection_point_float");
    }
    else if (old_domain == ATTR_DOMAIN_CURVE && domain == ATTR_DOMAIN_POINT) {
      VArray<float> point_selection = curves.adapt_domain(
          curves.selection_curve_float(), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT);
      point_selection.materialize(curves.selection_point_float_for_write());
      component.attribute_try_delete(".selection_curve_float");
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace set_selection_domain

static void CURVES_OT_set_selection_domain(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Set Select Mode";
  ot->idname = __func__;
  ot->description = "Change the mode used for selection masking in curves sculpt mode";

  ot->exec = set_selection_domain::curves_set_selection_domain_exec;
  ot->poll = selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(
      ot->srna, "domain", rna_enum_attribute_curves_domain_items, 0, "Domain", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
}

namespace disable_selection {

static int curves_disable_selection_exec(bContext *C, wmOperator *UNUSED(op))
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    curves_id->flag &= ~CV_SCULPT_SELECTION_ENABLED;

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace disable_selection

static void CURVES_OT_disable_selection(wmOperatorType *ot)
{
  ot->name = "Disable Selection";
  ot->idname = __func__;
  ot->description = "Disable the drawing of influence of selection in sculpt mode";

  ot->exec = disable_selection::curves_disable_selection_exec;
  ot->poll = selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool varray_contains_nonzero(const VArray<float> &data)
{
  bool contains_nonzero = false;
  devirtualize_varray(data, [&](const auto array) {
    for (const int i : data.index_range()) {
      if (array[i] != 0.0f) {
        contains_nonzero = true;
        break;
      }
    }
  });
  return contains_nonzero;
}

static bool has_anything_selected(const Curves &curves_id)
{
  const CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  switch (curves_id.selection_domain) {
    case ATTR_DOMAIN_POINT:
      return varray_contains_nonzero(curves.selection_point_float());
    case ATTR_DOMAIN_CURVE:
      return varray_contains_nonzero(curves.selection_curve_float());
  }
  BLI_assert_unreachable();
  return false;
}

static bool any_point_selected(const CurvesGeometry &curves)
{
  return varray_contains_nonzero(curves.selection_point_float());
}

static bool any_point_selected(const Span<Curves *> curves_ids)
{
  for (const Curves *curves_id : curves_ids) {
    if (any_point_selected(CurvesGeometry::wrap(curves_id->geometry))) {
      return true;
    }
  }
  return false;
}

namespace select_all {

static void invert_selection(MutableSpan<float> selection)
{
  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      selection[i] = 1.0f - selection[i];
    }
  });
}

static int select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);

  if (action == SEL_TOGGLE) {
    action = any_point_selected(unique_curves) ? SEL_DESELECT : SEL_SELECT;
  }

  for (Curves *curves_id : unique_curves) {
    if (action == SEL_SELECT) {
      /* The optimization to avoid storing the selection when everything is selected causes too
       * many problems at the moment, since there is no proper visualization yet. Keep the code but
       * disable it for now. */
#if 0
      CurveComponent component;
      component.replace(curves_id, GeometryOwnershipType::Editable);
      component.attribute_try_delete(".selection_point_float");
      component.attribute_try_delete(".selection_curve_float");
#else
      CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
      MutableSpan<float> selection = curves_id->selection_domain == ATTR_DOMAIN_POINT ?
                                         curves.selection_point_float_for_write() :
                                         curves.selection_curve_float_for_write();
      selection.fill(1.0f);
#endif
    }
    else {
      CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
      MutableSpan<float> selection = curves_id->selection_domain == ATTR_DOMAIN_POINT ?
                                         curves.selection_point_float_for_write() :
                                         curves.selection_curve_float_for_write();
      if (action == SEL_DESELECT) {
        selection.fill(0.0f);
      }
      else if (action == SEL_INVERT) {
        invert_selection(selection);
      }
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

}  // namespace select_all

static void SCULPT_CURVES_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->idname = __func__;
  ot->description = "(De)select all control points";

  ot->exec = select_all::select_all_exec;
  ot->poll = selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

namespace select_random {

static int select_random_exec(bContext *C, wmOperator *op)
{
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);

  const int seed = RNA_int_get(op->ptr, "seed");
  RandomNumberGenerator rng{static_cast<uint32_t>(seed)};

  const bool partial = RNA_boolean_get(op->ptr, "partial");
  const bool constant_per_curve = RNA_boolean_get(op->ptr, "constant_per_curve");
  const float probability = RNA_float_get(op->ptr, "probability");
  const float min_value = RNA_float_get(op->ptr, "min");
  const auto next_partial_random_value = [&]() {
    return rng.get_float() * (1.0f - min_value) + min_value;
  };
  const auto next_bool_random_value = [&]() { return rng.get_float() <= probability; };

  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
    const bool was_anything_selected = has_anything_selected(*curves_id);
    switch (curves_id->selection_domain) {
      case ATTR_DOMAIN_POINT: {
        MutableSpan<float> selection = curves.selection_point_float_for_write();
        if (!was_anything_selected) {
          selection.fill(1.0f);
        }
        if (partial) {
          if (constant_per_curve) {
            for (const int curve_i : curves.curves_range()) {
              const float random_value = next_partial_random_value();
              const IndexRange points = curves.points_for_curve(curve_i);
              for (const int point_i : points) {
                selection[point_i] *= random_value;
              }
            }
          }
          else {
            for (const int point_i : selection.index_range()) {
              const float random_value = next_partial_random_value();
              selection[point_i] *= random_value;
            }
          }
        }
        else {
          if (constant_per_curve) {
            for (const int curve_i : curves.curves_range()) {
              const bool random_value = next_bool_random_value();
              const IndexRange points = curves.points_for_curve(curve_i);
              if (!random_value) {
                selection.slice(points).fill(0.0f);
              }
            }
          }
          else {
            for (const int point_i : selection.index_range()) {
              const bool random_value = next_bool_random_value();
              if (!random_value) {
                selection[point_i] = 0.0f;
              }
            }
          }
        }
        break;
      }
      case ATTR_DOMAIN_CURVE: {
        MutableSpan<float> selection = curves.selection_curve_float_for_write();
        if (!was_anything_selected) {
          selection.fill(1.0f);
        }
        if (partial) {
          for (const int curve_i : curves.curves_range()) {
            const float random_value = next_partial_random_value();
            selection[curve_i] *= random_value;
          }
        }
        else {
          for (const int curve_i : curves.curves_range()) {
            const bool random_value = next_bool_random_value();
            if (!random_value) {
              selection[curve_i] = 0.0f;
            }
          }
        }
        break;
      }
    }
    MutableSpan<float> selection = curves_id->selection_domain == ATTR_DOMAIN_POINT ?
                                       curves.selection_point_float_for_write() :
                                       curves.selection_curve_float_for_write();
    const bool was_any_selected = std::any_of(
        selection.begin(), selection.end(), [](const float v) { return v > 0.0f; });
    if (was_any_selected) {
      for (float &v : selection) {
        v *= rng.get_float();
      }
    }
    else {
      for (float &v : selection) {
        v = rng.get_float();
      }
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

static void select_random_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;

  uiItemR(layout, op->ptr, "seed", 0, nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "constant_per_curve", 0, nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "partial", 0, nullptr, ICON_NONE);

  if (RNA_boolean_get(op->ptr, "partial")) {
    uiItemR(layout, op->ptr, "min", UI_ITEM_R_SLIDER, "Min", ICON_NONE);
  }
  else {
    uiItemR(layout, op->ptr, "probability", UI_ITEM_R_SLIDER, "Probability", ICON_NONE);
  }
}

}  // namespace select_random

static void SCULPT_CURVES_OT_select_random(wmOperatorType *ot)
{
  ot->name = "Select Random";
  ot->idname = __func__;
  ot->description = "Randomizes existing selection or create new random selection";

  ot->exec = select_random::select_random_exec;
  ot->poll = selection_poll;
  ot->ui = select_random::select_random_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "seed",
              0,
              INT32_MIN,
              INT32_MAX,
              "Seed",
              "Source of randomness",
              INT32_MIN,
              INT32_MAX);
  RNA_def_boolean(
      ot->srna, "partial", false, "Partial", "Allow points or curves to be selected partially");
  RNA_def_float(ot->srna,
                "probability",
                0.5f,
                0.0f,
                1.0f,
                "Probability",
                "Chance of every point or curve to be included in the selection",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "min",
                0.0f,
                0.0f,
                1.0f,
                "Min",
                "Minimum value for the random selection",
                0.0f,
                1.0f);
  RNA_def_boolean(ot->srna,
                  "constant_per_curve",
                  true,
                  "Constant per Curve",
                  "The generated random number is the same for every control point of a curve");
}

namespace select_end {
static bool select_end_poll(bContext *C)
{
  if (!selection_poll(C)) {
    return false;
  }
  const Curves *curves_id = static_cast<const Curves *>(CTX_data_active_object(C)->data);
  if (curves_id->selection_domain != ATTR_DOMAIN_POINT) {
    return false;
  }
  return true;
}

static int select_end_exec(bContext *C, wmOperator *op)
{
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);
  const bool end_points = RNA_boolean_get(op->ptr, "end_points");
  const int amount = RNA_int_get(op->ptr, "amount");

  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
    const bool was_anything_selected = has_anything_selected(*curves_id);
    MutableSpan<float> selection = curves.selection_point_float_for_write();
    if (!was_anything_selected) {
      selection.fill(1.0f);
    }
    threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = curves.points_for_curve(curve_i);
        IndexRange points_to_select;
        if (end_points) {
          selection.slice(points.drop_back(amount)).fill(0.0f);
        }
        else {
          selection.slice(points.drop_front(amount)).fill(0.0f);
        }
      }
    });

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}
}  // namespace select_end

static void SCULPT_CURVES_OT_select_end(wmOperatorType *ot)
{
  ot->name = "Select End";
  ot->idname = __func__;
  ot->description = "Select end points of curves";

  ot->exec = select_end::select_end_exec;
  ot->poll = select_end::select_end_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "end_points",
                  true,
                  "End Points",
                  "Select points at the end of the curve as opposed to the beginning");
  RNA_def_int(
      ot->srna, "amount", 1, 0, INT32_MAX, "Amount", "Number of points to select", 0, INT32_MAX);
}

namespace select_grow {

struct GrowOperatorDataPerCurve : NonCopyable, NonMovable {
  Curves *curves_id;
  Vector<int> selected_point_indices;
  Vector<int> unselected_point_indices;
  Array<float> distances_to_selected;
  Array<float> distances_to_unselected;

  Array<float> original_selection;
  float pixel_to_distance_factor;
};

struct GrowOperatorData {
  int initial_mouse_x;
  Vector<std::unique_ptr<GrowOperatorDataPerCurve>> per_curve;
};

static void update_points_selection(GrowOperatorDataPerCurve &data,
                                    const float distance,
                                    MutableSpan<float> points_selection)
{
  if (distance > 0) {
    threading::parallel_for(
        data.unselected_point_indices.index_range(), 256, [&](const IndexRange range) {
          for (const int i : range) {
            const int point_i = data.unselected_point_indices[i];
            const float distance_to_selected = data.distances_to_selected[i];
            const float selection = distance_to_selected <= distance ? 1.0f : 0.0f;
            points_selection[point_i] = selection;
          }
        });
    threading::parallel_for(
        data.selected_point_indices.index_range(), 512, [&](const IndexRange range) {
          for (const int point_i : data.selected_point_indices.as_span().slice(range)) {
            points_selection[point_i] = 1.0f;
          }
        });
  }
  else {
    threading::parallel_for(
        data.selected_point_indices.index_range(), 256, [&](const IndexRange range) {
          for (const int i : range) {
            const int point_i = data.selected_point_indices[i];
            const float distance_to_unselected = data.distances_to_unselected[i];
            const float selection = distance_to_unselected <= -distance ? 0.0f : 1.0f;
            points_selection[point_i] = selection;
          }
        });
    threading::parallel_for(
        data.unselected_point_indices.index_range(), 512, [&](const IndexRange range) {
          for (const int point_i : data.unselected_point_indices.as_span().slice(range)) {
            points_selection[point_i] = 0.0f;
          }
        });
  }
}

static int select_grow_update(bContext *C, wmOperator *op, const float mouse_diff_x)
{
  GrowOperatorData &op_data = *static_cast<GrowOperatorData *>(op->customdata);

  for (std::unique_ptr<GrowOperatorDataPerCurve> &curve_op_data : op_data.per_curve) {
    Curves &curves_id = *curve_op_data->curves_id;
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    const float distance = curve_op_data->pixel_to_distance_factor * mouse_diff_x;

    switch (curves_id.selection_domain) {
      case ATTR_DOMAIN_POINT: {
        MutableSpan<float> points_selection = curves.selection_point_float_for_write();
        update_points_selection(*curve_op_data, distance, points_selection);
        break;
      }
      case ATTR_DOMAIN_CURVE: {
        Array<float> new_points_selection(curves.points_num());
        update_points_selection(*curve_op_data, distance, new_points_selection);
        MutableSpan<float> curves_selection = curves.selection_curve_float_for_write();
        for (const int curve_i : curves.curves_range()) {
          const IndexRange points = curves.points_for_curve(curve_i);
          const Span<float> points_selection = new_points_selection.as_span().slice(points);
          const float max_selection = *std::max_element(points_selection.begin(),
                                                        points_selection.end());
          curves_selection[curve_i] = max_selection;
        }
        break;
      }
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &curves_id);
  }

  return OPERATOR_FINISHED;
}

static int select_grow_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  GrowOperatorData *op_data = MEM_new<GrowOperatorData>(__func__);
  op->customdata = op_data;

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    auto curve_op_data = std::make_unique<GrowOperatorDataPerCurve>();
    curve_op_data->curves_id = curves_id;
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
    const Span<float3> positions = curves.positions();

    switch (curves_id->selection_domain) {
      case ATTR_DOMAIN_POINT: {
        const VArray<float> points_selection = curves.selection_point_float();
        curve_op_data->original_selection.reinitialize(points_selection.size());
        points_selection.materialize(curve_op_data->original_selection);
        for (const int point_i : points_selection.index_range()) {
          const float point_selection = points_selection[point_i];
          if (point_selection > 0) {
            curve_op_data->selected_point_indices.append(point_i);
          }
          else {
            curve_op_data->unselected_point_indices.append(point_i);
          }
        }

        break;
      }
      case ATTR_DOMAIN_CURVE: {
        const VArray<float> curves_selection = curves.selection_curve_float();
        curve_op_data->original_selection.reinitialize(curves_selection.size());
        curves_selection.materialize(curve_op_data->original_selection);
        for (const int curve_i : curves_selection.index_range()) {
          const float curve_selection = curves_selection[curve_i];
          const IndexRange points = curves.points_for_curve(curve_i);
          if (curve_selection > 0) {
            for (const int point_i : points) {
              curve_op_data->selected_point_indices.append(point_i);
            }
          }
          else {
            for (const int point_i : points) {
              curve_op_data->unselected_point_indices.append(point_i);
            }
          }
        }
        break;
      }
    }

    threading::parallel_invoke(
        [&]() {
          KDTree_3d *kdtree = BLI_kdtree_3d_new(curve_op_data->selected_point_indices.size());
          BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(kdtree); });
          for (const int point_i : curve_op_data->selected_point_indices) {
            const float3 &position = positions[point_i];
            BLI_kdtree_3d_insert(kdtree, point_i, position);
          }
          BLI_kdtree_3d_balance(kdtree);

          curve_op_data->distances_to_selected.reinitialize(
              curve_op_data->unselected_point_indices.size());

          threading::parallel_for(
              curve_op_data->unselected_point_indices.index_range(),
              256,
              [&](const IndexRange range) {
                for (const int i : range) {
                  const int point_i = curve_op_data->unselected_point_indices[i];
                  const float3 &position = positions[point_i];
                  KDTreeNearest_3d nearest;
                  BLI_kdtree_3d_find_nearest(kdtree, position, &nearest);
                  curve_op_data->distances_to_selected[i] = nearest.dist;
                }
              });
        },
        [&]() {
          KDTree_3d *kdtree = BLI_kdtree_3d_new(curve_op_data->unselected_point_indices.size());
          BLI_SCOPED_DEFER([&]() { BLI_kdtree_3d_free(kdtree); });
          for (const int point_i : curve_op_data->unselected_point_indices) {
            const float3 &position = positions[point_i];
            BLI_kdtree_3d_insert(kdtree, point_i, position);
          }
          BLI_kdtree_3d_balance(kdtree);

          curve_op_data->distances_to_unselected.reinitialize(
              curve_op_data->selected_point_indices.size());

          threading::parallel_for(curve_op_data->selected_point_indices.index_range(),
                                  256,
                                  [&](const IndexRange range) {
                                    for (const int i : range) {
                                      const int point_i = curve_op_data->selected_point_indices[i];
                                      const float3 &position = positions[point_i];
                                      KDTreeNearest_3d nearest;
                                      BLI_kdtree_3d_find_nearest(kdtree, position, &nearest);
                                      curve_op_data->distances_to_unselected[i] = nearest.dist;
                                    }
                                  });
        });

    /* TODO */
    Object *ob = CTX_data_active_object(C);

    ARegion *region = CTX_wm_region(C);
    View3D *v3d = CTX_wm_view3d(C);
    float4x4 projection;
    ED_view3d_ob_project_mat_get(CTX_wm_region_view3d(C), ob, projection.values);

    float4x4 curves_to_world_mat = ob->obmat;
    float4x4 world_to_curves_mat = curves_to_world_mat.inverted();

    curve_op_data->pixel_to_distance_factor = threading::parallel_reduce(
        curve_op_data->selected_point_indices.index_range(),
        256,
        FLT_MAX,
        [&](const IndexRange range, float pixel_to_distance_factor) {
          for (const int i : range) {
            const int point_i = curve_op_data->selected_point_indices[i];
            const float3 &pos_cu = positions[point_i];
            float2 pos_re;
            ED_view3d_project_float_v2_m4(region, pos_cu, pos_re, projection.values);
            if (pos_re.x < 0 || pos_re.y < 0 || pos_re.x > region->winx ||
                pos_re.y > region->winy) {
              continue;
            }
            const float2 pos_offset_re = pos_re + float2(1, 0);
            float3 pos_offset_wo;
            ED_view3d_win_to_3d(
                v3d, region, curves_to_world_mat * pos_cu, pos_offset_re, pos_offset_wo);
            const float3 pos_offset_cu = world_to_curves_mat * pos_offset_wo;
            const float dist_cu = math::distance(pos_cu, pos_offset_cu);
            const float dist_re = math::distance(pos_re, pos_offset_re);
            const float factor = dist_cu / dist_re;
            math::min_inplace(pixel_to_distance_factor, factor);
          }
          return pixel_to_distance_factor;
        },
        [](const float a, const float b) { return std::min(a, b); });

    op_data->per_curve.append(std::move(curve_op_data));
  }

  op_data->initial_mouse_x = event->mval[0];

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int select_grow_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  GrowOperatorData &op_data = *static_cast<GrowOperatorData *>(op->customdata);
  const int mouse_x = event->mval[0];
  const int mouse_diff_x = mouse_x - op_data.initial_mouse_x;
  switch (event->type) {
    case MOUSEMOVE: {
      select_grow_update(C, op, mouse_diff_x);
      break;
    }
    case LEFTMOUSE: {
      MEM_delete(&op_data);
      return OPERATOR_FINISHED;
    }
    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      for (std::unique_ptr<GrowOperatorDataPerCurve> &curve_op_data : op_data.per_curve) {
        Curves &curves_id = *curve_op_data->curves_id;
        CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
        switch (curves_id.selection_domain) {
          case ATTR_DOMAIN_POINT: {
            MutableSpan<float> points_selection = curves.selection_point_float_for_write();
            points_selection.copy_from(curve_op_data->original_selection);
            break;
          }
          case ATTR_DOMAIN_CURVE: {
            MutableSpan<float> curves_seletion = curves.selection_curve_float_for_write();
            curves_seletion.copy_from(curve_op_data->original_selection);
            break;
          }
        }

        /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
         * attribute for now. */
        DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, &curves_id);
      }
      MEM_delete(&op_data);
      return OPERATOR_CANCELLED;
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

}  // namespace select_grow

static void SCULPT_CURVES_OT_select_grow(wmOperatorType *ot)
{
  ot->name = "Select Grow";
  ot->idname = __func__;
  ot->description = "Select curves which are close to curves that are selected already";

  ot->invoke = select_grow::select_grow_invoke;
  ot->modal = select_grow::select_grow_modal;
  ot->poll = selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_float(ot->srna,
                       "distance",
                       0.1f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Distance",
                       "By how much to grow the selection",
                       -10.0f,
                       10.f);
  RNA_def_property_subtype(prop, PROP_DISTANCE);
}

namespace min_distance_edit {

static bool min_distance_edit_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type != OB_CURVES) {
    return false;
  }
  Curves *curves_id = static_cast<Curves *>(ob->data);
  if (curves_id->surface == nullptr) {
    return false;
  }
  if (curves_id->surface->type != OB_MESH) {
    return false;
  }
  Scene *scene = CTX_data_scene(C);
  const Brush *brush = BKE_paint_brush_for_read(&scene->toolsettings->curves_sculpt->paint);
  if (brush == nullptr) {
    return false;
  }
  if (brush->curves_sculpt_tool != CURVES_SCULPT_TOOL_DENSITY) {
    return false;
  }
  return true;
}

struct MinDistanceEditData {
  Brush *brush;
  float4x4 curves_to_world_mat;
  float3 pos_cu;
  float3 normal_cu;
  int initial_mouse_x;
  float initial_minimum_distance;
  void *draw_handle;
};

static void min_distance_edit_draw(const bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
  MinDistanceEditData &op_data = *static_cast<MinDistanceEditData *>(arg);

  const float min_distance = op_data.brush->curves_sculpt_settings->minimum_distance;

  float3 tangent_x_cu = math::cross(op_data.normal_cu, float3{0, 0, 1});
  if (math::is_zero(tangent_x_cu)) {
    tangent_x_cu = math::cross(op_data.normal_cu, float3{0, 1, 0});
  }
  tangent_x_cu = math::normalize(tangent_x_cu);
  const float3 tangent_y_cu = math::normalize(math::cross(op_data.normal_cu, tangent_x_cu));

  const int points_per_side = 4;
  const int points_per_axis_num = 2 * points_per_side + 1;

  Vector<float3> points_wo;
  for (const int x_i : IndexRange(points_per_axis_num)) {
    for (const int y_i : IndexRange(points_per_axis_num)) {
      const float x = min_distance * (x_i - (points_per_axis_num - 1) / 2.0f);
      const float y = min_distance * (y_i - (points_per_axis_num - 1) / 2.0f);

      const float3 point_pos_cu = op_data.pos_cu + op_data.normal_cu * 0.0001f + x * tangent_x_cu +
                                  y * tangent_y_cu;
      const float3 point_pos_wo = op_data.curves_to_world_mat * point_pos_cu;
      points_wo.append(point_pos_wo);
    }
  }

  const uint pos3d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_point_size(3);
  immUniformColor4f(0.9f, 0.9f, 0.9f, 1.0f);
  immBegin(GPU_PRIM_POINTS, points_wo.size());
  for (const float3 &pos_wo : points_wo) {
    immVertex3fv(pos3d, pos_wo);
  }
  immEnd();

  GPU_point_size(1);
}

static int min_distance_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);

  Object &curves_ob = *CTX_data_active_object(C);
  Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  Object &surface_ob = *curves_id.surface;
  Mesh &surface_me = *static_cast<Mesh *>(surface_ob.data);

  BVHTreeFromMesh surface_bvh;
  BKE_bvhtree_from_mesh_get(&surface_bvh, &surface_me, BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

  const int2 mouse_pos_int_re{event->mval};
  const float2 mouse_pos_re{mouse_pos_int_re};

  float3 ray_start_wo, ray_end_wo;
  ED_view3d_win_to_segment_clipped(
      depsgraph, region, v3d, mouse_pos_re, ray_start_wo, ray_end_wo, true);

  const float4x4 surface_to_world_mat = surface_ob.obmat;
  const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();

  const float3 ray_start_su = world_to_surface_mat * ray_start_wo;
  const float3 ray_end_su = world_to_surface_mat * ray_end_wo;
  const float3 ray_direction_su = math::normalize(ray_end_su - ray_start_su);

  BVHTreeRayHit ray_hit;
  ray_hit.dist = FLT_MAX;
  ray_hit.index = -1;
  BLI_bvhtree_ray_cast(surface_bvh.tree,
                       ray_start_su,
                       ray_direction_su,
                       0.0f,
                       &ray_hit,
                       surface_bvh.raycast_callback,
                       &surface_bvh);
  if (ray_hit.index == -1) {
    return OPERATOR_CANCELLED;
  }

  const float3 hit_pos_su = ray_hit.co;
  const float3 hit_normal_su = ray_hit.no;
  const float4x4 curves_to_world_mat = curves_ob.obmat;
  const float4x4 world_to_curves_mat = curves_to_world_mat.inverted();
  const float4x4 surface_to_curves_mat = world_to_curves_mat * surface_to_world_mat;
  const float4x4 surface_to_curves_normal_mat = surface_to_curves_mat.inverted().transposed();

  const float3 hit_pos_cu = surface_to_curves_mat * hit_pos_su;
  const float3 hit_normal_cu = math::normalize(surface_to_curves_normal_mat * hit_normal_su);

  MinDistanceEditData *op_data = MEM_new<MinDistanceEditData>(__func__);
  op_data->curves_to_world_mat = curves_to_world_mat;
  op_data->normal_cu = hit_normal_cu;
  op_data->pos_cu = hit_pos_cu;
  op_data->initial_mouse_x = mouse_pos_int_re.x;
  op_data->draw_handle = ED_region_draw_cb_activate(
      region->type, min_distance_edit_draw, op_data, REGION_DRAW_POST_VIEW);
  op_data->brush = BKE_paint_brush(&scene->toolsettings->curves_sculpt->paint);
  op_data->initial_minimum_distance = op_data->brush->curves_sculpt_settings->minimum_distance;

  if (op_data->initial_minimum_distance <= 0.0f) {
    op_data->initial_minimum_distance = 0.01f;
  }

  op->customdata = op_data;
  WM_event_add_modal_handler(C, op);
  ED_region_tag_redraw(region);
  return OPERATOR_RUNNING_MODAL;
}

static int min_distance_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  MinDistanceEditData &op_data = *static_cast<MinDistanceEditData *>(op->customdata);

  auto finish = [&]() {
    ED_region_tag_redraw(region);
    ED_region_draw_cb_exit(region->type, op_data.draw_handle);
    MEM_freeN(&op_data);
  };

  switch (event->type) {
    case MOUSEMOVE: {
      const int2 mouse_pos_int_re{event->mval};
      const float2 mouse_pos_re{mouse_pos_int_re};

      const float mouse_diff_x = mouse_pos_int_re.x - op_data.initial_mouse_x;
      const float factor = powf(2, mouse_diff_x / UI_UNIT_X / 10.0f);
      op_data.brush->curves_sculpt_settings->minimum_distance = op_data.initial_minimum_distance *
                                                                factor;

      ED_region_tag_redraw(region);
      WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
      break;
    }
    case LEFTMOUSE: {
      if (event->val == KM_PRESS) {
        finish();
        return OPERATOR_FINISHED;
      }
      break;
    }
    case RIGHTMOUSE:
    case EVT_ESCKEY: {
      op_data.brush->curves_sculpt_settings->minimum_distance = op_data.initial_minimum_distance;
      finish();
      WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

}  // namespace min_distance_edit

static void SCULPT_CURVES_OT_min_distance_edit(wmOperatorType *ot)
{
  ot->name = "Edit Minimum Distance";
  ot->idname = __func__;
  ot->description = "Change the minimum distance used by the density brush";

  ot->poll = min_distance_edit::min_distance_edit_poll;
  ot->invoke = min_distance_edit::min_distance_edit_invoke;
  ot->modal = min_distance_edit::min_distance_edit_modal;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;
}

}  // namespace blender::ed::curves

void ED_operatortypes_curves()
{
  using namespace blender::ed::curves;
  WM_operatortype_append(CURVES_OT_convert_to_particle_system);
  WM_operatortype_append(CURVES_OT_convert_from_particle_system);
  WM_operatortype_append(CURVES_OT_snap_curves_to_surface);
  WM_operatortype_append(CURVES_OT_set_selection_domain);
  WM_operatortype_append(SCULPT_CURVES_OT_select_all);
  WM_operatortype_append(SCULPT_CURVES_OT_select_random);
  WM_operatortype_append(SCULPT_CURVES_OT_select_end);
  WM_operatortype_append(SCULPT_CURVES_OT_select_grow);
  WM_operatortype_append(CURVES_OT_disable_selection);
  WM_operatortype_append(SCULPT_CURVES_OT_min_distance_edit);
}
