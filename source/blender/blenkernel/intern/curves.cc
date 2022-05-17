/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_bounds.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_curves.hh"
#include "BKE_customdata.h"
#include "BKE_geometry_set.hh"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"

#include "BLT_translation.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::RandomNumberGenerator;
using blender::Span;

static const char *ATTR_POSITION = "position";

static void update_custom_data_pointers(Curves &curves);

static void curves_init_data(ID *id)
{
  Curves *curves = (Curves *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(curves, id));

  MEMCPY_STRUCT_AFTER(curves, DNA_struct_default_get(Curves), id);

  new (&curves->geometry) blender::bke::CurvesGeometry();
}

static void curves_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  using namespace blender;

  Curves *curves_dst = (Curves *)id_dst;
  const Curves *curves_src = (const Curves *)id_src;
  curves_dst->mat = static_cast<Material **>(MEM_dupallocN(curves_src->mat));

  const bke::CurvesGeometry &src = bke::CurvesGeometry::wrap(curves_src->geometry);
  bke::CurvesGeometry &dst = bke::CurvesGeometry::wrap(curves_dst->geometry);

  /* We need special handling here because the generic ID management code has already done a
   * shallow copy from the source to the destination, and because the copy-on-write functionality
   * isn't supported more generically yet. */

  dst.point_num = src.point_num;
  dst.curve_num = src.curve_num;

  const eCDAllocType alloc_type = (flag & LIB_ID_COPY_CD_REFERENCE) ? CD_REFERENCE : CD_DUPLICATE;
  CustomData_copy(&src.point_data, &dst.point_data, CD_MASK_ALL, alloc_type, dst.point_num);
  CustomData_copy(&src.curve_data, &dst.curve_data, CD_MASK_ALL, alloc_type, dst.curve_num);

  dst.curve_offsets = static_cast<int *>(MEM_dupallocN(src.curve_offsets));

  dst.runtime = MEM_new<bke::CurvesGeometryRuntime>(__func__);

  dst.runtime->type_counts = src.runtime->type_counts;

  dst.update_customdata_pointers();

  curves_dst->batch_cache = nullptr;
}

static void curves_free_data(ID *id)
{
  Curves *curves = (Curves *)id;
  BKE_animdata_free(&curves->id, false);

  blender::bke::CurvesGeometry::wrap(curves->geometry).~CurvesGeometry();

  BKE_curves_batch_cache_free(curves);

  MEM_SAFE_FREE(curves->mat);
}

static void curves_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Curves *curves = (Curves *)id;
  for (int i = 0; i < curves->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, curves->mat[i], IDWALK_CB_USER);
  }
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, curves->surface, IDWALK_CB_NOP);
}

static void curves_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Curves *curves = (Curves *)id;

  CustomDataLayer *players = nullptr, players_buff[CD_TEMP_CHUNK_SIZE];
  CustomDataLayer *clayers = nullptr, clayers_buff[CD_TEMP_CHUNK_SIZE];
  CustomData_blend_write_prepare(
      &curves->geometry.point_data, &players, players_buff, ARRAY_SIZE(players_buff));
  CustomData_blend_write_prepare(
      &curves->geometry.curve_data, &clayers, clayers_buff, ARRAY_SIZE(clayers_buff));

  /* Write LibData */
  BLO_write_id_struct(writer, Curves, id_address, &curves->id);
  BKE_id_blend_write(writer, &curves->id);

  /* Direct data */
  CustomData_blend_write(writer,
                         &curves->geometry.point_data,
                         players,
                         curves->geometry.point_num,
                         CD_MASK_ALL,
                         &curves->id);
  CustomData_blend_write(writer,
                         &curves->geometry.curve_data,
                         clayers,
                         curves->geometry.curve_num,
                         CD_MASK_ALL,
                         &curves->id);

  BLO_write_int32_array(writer, curves->geometry.curve_num + 1, curves->geometry.curve_offsets);

  BLO_write_pointer_array(writer, curves->totcol, curves->mat);
  if (curves->adt) {
    BKE_animdata_blend_write(writer, curves->adt);
  }

  /* Remove temporary data. */
  if (players && players != players_buff) {
    MEM_freeN(players);
  }
  if (clayers && clayers != clayers_buff) {
    MEM_freeN(clayers);
  }
}

static void curves_blend_read_data(BlendDataReader *reader, ID *id)
{
  Curves *curves = (Curves *)id;
  BLO_read_data_address(reader, &curves->adt);
  BKE_animdata_blend_read_data(reader, curves->adt);

  /* Geometry */
  CustomData_blend_read(reader, &curves->geometry.point_data, curves->geometry.point_num);
  CustomData_blend_read(reader, &curves->geometry.curve_data, curves->geometry.curve_num);
  update_custom_data_pointers(*curves);

  BLO_read_int32_array(reader, curves->geometry.curve_num + 1, &curves->geometry.curve_offsets);

  curves->geometry.runtime = MEM_new<blender::bke::CurvesGeometryRuntime>(__func__);

  /* Recalculate curve type count cache that isn't saved in files. */
  blender::bke::CurvesGeometry::wrap(curves->geometry).update_curve_types();

  /* Materials */
  BLO_read_pointer_array(reader, (void **)&curves->mat);
}

static void curves_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Curves *curves = (Curves *)id;
  for (int a = 0; a < curves->totcol; a++) {
    BLO_read_id_address(reader, curves->id.lib, &curves->mat[a]);
  }
  BLO_read_id_address(reader, curves->id.lib, &curves->surface);
}

static void curves_blend_read_expand(BlendExpander *expander, ID *id)
{
  Curves *curves = (Curves *)id;
  for (int a = 0; a < curves->totcol; a++) {
    BLO_expand(expander, curves->mat[a]);
  }
  BLO_expand(expander, curves->surface);
}

IDTypeInfo IDType_ID_CV = {
    /*id_code */ ID_CV,
    /*id_filter */ FILTER_ID_CV,
    /*main_listbase_index */ INDEX_ID_CV,
    /*struct_size */ sizeof(Curves),
    /*name */ "Hair Curves",
    /*name_plural */ "Hair Curves",
    /*translation_context */ BLT_I18NCONTEXT_ID_CURVES,
    /*flags */ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info */ nullptr,

    /*init_data */ curves_init_data,
    /*copy_data */ curves_copy_data,
    /*free_data */ curves_free_data,
    /*make_local */ nullptr,
    /*foreach_id */ curves_foreach_id,
    /*foreach_cache */ nullptr,
    /*foreach_path */ nullptr,
    /*owner_get */ nullptr,

    /*blend_write */ curves_blend_write,
    /*blend_read_data */ curves_blend_read_data,
    /*blend_read_lib */ curves_blend_read_lib,
    /*blend_read_expand */ curves_blend_read_expand,

    /*blend_read_undo_preserve */ nullptr,

    /*lib_override_apply_post */ nullptr,
};

static void update_custom_data_pointers(Curves &curves)
{
  blender::bke::CurvesGeometry::wrap(curves.geometry).update_customdata_pointers();
}

void *BKE_curves_add(Main *bmain, const char *name)
{
  Curves *curves = static_cast<Curves *>(BKE_id_new(bmain, ID_CV, name));

  return curves;
}

BoundBox *BKE_curves_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVES);
  const Curves *curves_id = static_cast<const Curves *>(ob->data);

  if (ob->runtime.bb != nullptr && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = MEM_cnew<BoundBox>(__func__);

    const blender::bke::CurvesGeometry &curves = blender::bke::CurvesGeometry::wrap(
        curves_id->geometry);

    float3 min(FLT_MAX);
    float3 max(-FLT_MAX);
    if (!curves.bounds_min_max(min, max)) {
      min = float3(-1);
      max = float3(1);
    }

    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
  }

  return ob->runtime.bb;
}

bool BKE_curves_customdata_required(Curves *UNUSED(curves), CustomDataLayer *layer)
{
  return layer->type == CD_PROP_FLOAT3 && STREQ(layer->name, ATTR_POSITION);
}

Curves *BKE_curves_copy_for_eval(Curves *curves_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  Curves *result = (Curves *)BKE_id_copy_ex(nullptr, &curves_src->id, nullptr, flags);
  return result;
}

static void curves_evaluate_modifiers(struct Depsgraph *depsgraph,
                                      struct Scene *scene,
                                      Object *object,
                                      GeometrySet &geometry_set)
{
  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  ModifierApplyFlag apply_flag = use_render ? MOD_APPLY_RENDER : MOD_APPLY_USECACHE;
  const ModifierEvalContext mectx = {depsgraph, object, apply_flag};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtualModifierData);

  /* Evaluate modifiers. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if (mti->modifyGeometrySet != nullptr) {
      mti->modifyGeometrySet(md, &mectx, &geometry_set);
    }
  }
}

void BKE_curves_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  Curves *curves = static_cast<Curves *>(object->data);
  GeometrySet geometry_set = GeometrySet::create_with_curves(curves,
                                                             GeometryOwnershipType::ReadOnly);
  curves_evaluate_modifiers(depsgraph, scene, object, geometry_set);

  /* Assign evaluated object. */
  Curves *curves_eval = const_cast<Curves *>(geometry_set.get_curves_for_read());
  if (curves_eval == nullptr) {
    curves_eval = blender::bke::curves_new_nomain(0, 0);
    BKE_object_eval_assign_data(object, &curves_eval->id, true);
  }
  else {
    BKE_object_eval_assign_data(object, &curves_eval->id, false);
  }
  object->runtime.geometry_set_eval = new GeometrySet(std::move(geometry_set));
}

/* Draw Cache */

void (*BKE_curves_batch_cache_dirty_tag_cb)(Curves *curves, int mode) = nullptr;
void (*BKE_curves_batch_cache_free_cb)(Curves *curves) = nullptr;

void BKE_curves_batch_cache_dirty_tag(Curves *curves, int mode)
{
  if (curves->batch_cache) {
    BKE_curves_batch_cache_dirty_tag_cb(curves, mode);
  }
}

void BKE_curves_batch_cache_free(Curves *curves)
{
  if (curves->batch_cache) {
    BKE_curves_batch_cache_free_cb(curves);
  }
}

namespace blender::bke {

Curves *curves_new_nomain(const int points_num, const int curves_num)
{
  Curves *curves_id = static_cast<Curves *>(BKE_id_new_nomain(ID_CV, nullptr));
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
  curves.resize(points_num, curves_num);
  return curves_id;
}

Curves *curves_new_nomain_single(const int points_num, const CurveType type)
{
  Curves *curves_id = curves_new_nomain(points_num, 1);
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
  curves.offsets_for_write().last() = points_num;
  curves.fill_curve_types(type);
  return curves_id;
}

Curves *curves_new_nomain(CurvesGeometry curves)
{
  Curves *curves_id = static_cast<Curves *>(BKE_id_new_nomain(ID_CV, nullptr));
  bke::CurvesGeometry::wrap(curves_id->geometry) = std::move(curves);
  return curves_id;
}

static float legacy_parameter_to_radius(const float shape,
                                        const float root,
                                        const float tip,
                                        const float t)
{
  BLI_assert(t >= 0.0f);
  BLI_assert(t <= 1.0f);
  float radius = 1.0f - t;

  if (shape != 0.0f) {
    if (shape < 0.0f)
      radius = powf(radius, 1.0f + shape);
    else
      radius = powf(radius, 1.0f / (1.0f - shape));
  }
  return (radius * (root - tip)) + tip;
}

void particle_hair_to_curves(Object &object, ParticleSystemModifierData &psmd, Curves &r_curves_id)
{
  ParticleSystem &psys = *psmd.psys;
  ParticleSettings &settings = *psys.part;
  Mesh &mesh = *psmd.mesh_final;
  if (psys.part->type != PART_HAIR) {
    return;
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
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(r_curves_id.geometry);
  curves.resize(points_num, curves_num);
  curves.offsets_for_write().copy_from(curve_offsets);

  if (curves_num == 0) {
    return;
  }

  const float4x4 object_to_world_mat = object.obmat;
  const float4x4 world_to_object_mat = object_to_world_mat.inverted();

  MutableSpan<float3> positions = curves.positions_for_write();

  CurveComponent curves_component;
  curves_component.replace(&r_curves_id, GeometryOwnershipType::Editable);
  bke::OutputAttribute_Typed radius_attr =
      curves_component.attribute_try_get_for_output_only<float>("radius", ATTR_DOMAIN_POINT);
  MutableSpan<float> radius_attr_span = radius_attr.as_span();

  blender::bke::LegacyHairSettings &legacy_hair_settings = curves.runtime->legacy_hair_settings;
  legacy_hair_settings.close_tip = (settings.shape_flag & PART_SHAPE_CLOSE_TIP) != 0;
  legacy_hair_settings.radius_shape = settings.shape;
  legacy_hair_settings.radius_root = settings.rad_root * settings.rad_scale * 0.5f;
  legacy_hair_settings.radius_tip = settings.rad_tip * settings.rad_scale * 0.5f;

  const auto copy_hair_to_curves = [&](const Span<ParticleCacheKey *> hair_cache,
                                       const Span<int> indices_to_transfer,
                                       const int curve_index_offset) {
    threading::parallel_for(indices_to_transfer.index_range(), 256, [&](const IndexRange range) {
      for (const int i : range) {
        const int hair_i = indices_to_transfer[i];
        const int curve_i = i + curve_index_offset;
        const IndexRange points = curves.points_for_curve(curve_i);
        const Span<ParticleCacheKey> keys{hair_cache[hair_i], points.size()};

        float curve_length = 0.0f;
        float3 prev_key_pos(0.0f);

        for (const int key_i : keys.index_range()) {
          const int point_i = points[key_i];
          const ParticleCacheKey &key = keys[key_i];
          const float3 key_pos = world_to_object_mat * float3(key.co);
          positions[point_i] = key_pos;

          if (key_i > 0) {
            curve_length += math::distance(key_pos, prev_key_pos);
          }
          radius_attr_span[point_i] = curve_length;
          prev_key_pos = key_pos;
        }

        /* Compute radius using normalized length. */
        for (const int key_i : keys.index_range()) {
          const int point_i = points[key_i];
          const float t = (curve_length == 0.0f) ? 0.0f : radius_attr_span[point_i] / curve_length;
          radius_attr_span[point_i] = legacy_parameter_to_radius(legacy_hair_settings.radius_shape,
                                                                 legacy_hair_settings.radius_root,
                                                                 legacy_hair_settings.radius_tip,
                                                                 t);
        }
        if (legacy_hair_settings.close_tip) {
          radius_attr_span[points.last()] = 0.0f;
        }
      }
    });
  };

  if (transfer_parents) {
    copy_hair_to_curves(parents_cache, parents_to_transfer, 0);
  }
  copy_hair_to_curves(children_cache, children_to_transfer, parents_to_transfer.size());

  radius_attr.save();

  BKE_mesh_tessface_ensure(&mesh);
  const int color_layer_offset = mesh.fdata.typemap[CD_MCOL];
  const int uv_layer_offset = mesh.fdata.typemap[CD_MTFACE];
  for (const int layer_index : IndexRange(mesh.fdata.totlayer)) {
    const CustomDataLayer &layer = mesh.fdata.layers[layer_index];
    if (layer.type == CD_MCOL) {
      bke::OutputAttribute_Typed<ColorGeometry4f> color_attr =
          curves_component.attribute_try_get_for_output_only<ColorGeometry4f>(layer.name,
                                                                              ATTR_DOMAIN_CURVE);
      MutableSpan<ColorGeometry4f> color_attr_span = color_attr.as_span();
      const int color_index = layer_index - color_layer_offset;
      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
        for (const int curve_i : range) {
          ColorGeometry4f &color = color_attr_span[curve_i];
          BKE_psys_mcol_on_emitter(&psys,
                                   nullptr,
                                   &psmd,
                                   /* Might be out of bounds, but the called function
                                    * checks for that using the next argument. */
                                   psys.particles + curve_i,
                                   curve_i,
                                   color_index,
                                   color);
        }
      });
      color_attr.save();
    }
    if (layer.type == CD_MTFACE) {
      bke::OutputAttribute_Typed<float2> uv_attr =
          curves_component.attribute_try_get_for_output_only<float2>(layer.name,
                                                                     ATTR_DOMAIN_CURVE);
      MutableSpan<float2> uv_attr_span = uv_attr.as_span();
      const int uv_index = layer_index - uv_layer_offset;
      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange range) {
        for (const int curve_i : range) {
          float2 &uv = uv_attr_span[curve_i];
          BKE_psys_uv_on_emitter(&psys,
                                 nullptr,
                                 &psmd,
                                 /* Might be out of bounds, but the called function
                                  * checks for that using the next argument. */
                                 psys.particles + curve_i,
                                 curve_i,
                                 uv_index,
                                 uv);
        }
      });
      uv_attr.save();
    }
  }
  for (const CustomDataLayer &layer : Span{mesh.fdata.layers, mesh.fdata.totlayer}) {
    if (layer.type != CD_MCOL) {
      continue;
    }
  }

  curves.update_curve_types();
  curves.tag_topology_changed();
}

}  // namespace blender::bke
