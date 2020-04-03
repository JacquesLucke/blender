/*
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
 */

/** \file
 * \ingroup modifiers
 */

#include <stddef.h>

#include "BLI_utildefines.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  pmd->canvas = NULL;
  pmd->brush = NULL;
  pmd->type = MOD_DYNAMICPAINT_TYPE_CANVAS;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const DynamicPaintModifierData *pmd = (const DynamicPaintModifierData *)md;
  DynamicPaintModifierData *tpmd = (DynamicPaintModifierData *)target;

  dynamicPaint_Modifier_copy(pmd, tpmd, flag);
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  DynamicPaintRuntime *runtime_data = (DynamicPaintRuntime *)runtime_data_v;
  dynamicPaint_Modifier_free_runtime(runtime_data);
}

static void freeData(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  dynamicPaint_Modifier_free(pmd);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface = pmd->canvas->surfaces.first;
    for (; surface; surface = surface->next) {
      /* tface */
      if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ ||
          surface->init_color_type == MOD_DPAINT_INITIAL_TEXTURE) {
        r_cddata_masks->lmask |= CD_MASK_MLOOPUV;
      }
      /* mcol */
      if (surface->type == MOD_DPAINT_SURFACE_T_PAINT ||
          surface->init_color_type == MOD_DPAINT_INITIAL_VERTEXCOLOR) {
        r_cddata_masks->lmask |= CD_MASK_MLOOPCOL;
      }
      /* CD_MDEFORMVERT */
      if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
        r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
      }
    }
  }
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface;
    BLO_write_struct(writer, DynamicPaintCanvasSettings, pmd->canvas);

    /* write surfaces */
    for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
      BLO_write_struct(writer, DynamicPaintSurface, surface);
    }
    /* write caches and effector weights */
    for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
      BKE_ptcache_blend_write_list(writer, &(surface->ptcaches));
      BLO_write_struct(writer, EffectorWeights, surface->effector_weights);
    }
  }
  if (pmd->brush) {
    BLO_write_struct(writer, DynamicPaintBrushSettings, pmd->brush);
    BLO_write_struct(writer, ColorBand, pmd->brush->paint_ramp);
    BLO_write_struct(writer, ColorBand, pmd->brush->vel_ramp);
  }
}

static void blendRead(BlendReader *reader, ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    BLO_read_data_address(reader, &pmd->canvas);
    pmd->canvas->pmd = pmd;
    pmd->canvas->flags &= ~MOD_DPAINT_BAKING; /* just in case */

    if (pmd->canvas->surfaces.first) {
      DynamicPaintSurface *surface;
      BLO_read_list(reader, &pmd->canvas->surfaces, NULL);

      for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
        surface->canvas = pmd->canvas;
        surface->data = NULL;
        BKE_ptcache_blend_read(reader, &(surface->ptcaches), &(surface->pointcache), 1);

        BLO_read_data_address(reader, &surface->effector_weights);
        if (surface->effector_weights == NULL) {
          surface->effector_weights = BKE_effector_add_weights(NULL);
        }
      }
    }
  }
  if (pmd->brush) {
    BLO_read_data_address(reader, &pmd->brush);
    pmd->brush->pmd = pmd;

    BLO_read_data_address(reader, &pmd->brush->psys);
    BLO_read_data_address(reader, &pmd->brush->paint_ramp);
    BLO_read_data_address(reader, &pmd->brush->vel_ramp);
  }
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  /* dont apply dynamic paint on orco mesh stack */
  if (!(ctx->flag & MOD_APPLY_ORCO)) {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    return dynamicPaint_Modifier_do(pmd, ctx->depsgraph, scene, ctx->object, mesh);
  }
  return mesh;
}

static bool is_brush_cb(Object *UNUSED(ob), ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  return (pmd->brush != NULL && pmd->type == MOD_DYNAMICPAINT_TYPE_BRUSH);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  /* Add relation from canvases to all brush objects. */
  if (pmd->canvas != NULL && pmd->type == MOD_DYNAMICPAINT_TYPE_CANVAS) {
    for (DynamicPaintSurface *surface = pmd->canvas->surfaces.first; surface;
         surface = surface->next) {
      if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
        DEG_add_forcefield_relations(
            ctx->node, ctx->object, surface->effector_weights, true, 0, "Dynamic Paint Field");
      }

      /* Actual code uses custom loop over group/scene
       * without layer checks in dynamicPaint_doStep. */
      DEG_add_collision_relations(ctx->node,
                                  ctx->object,
                                  surface->brush_group,
                                  eModifierType_DynamicPaint,
                                  is_brush_cb,
                                  "Dynamic Paint Brush");
    }
  }
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
  return true;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface = pmd->canvas->surfaces.first;

    for (; surface; surface = surface->next) {
      walk(userData, ob, (ID **)&surface->brush_group, IDWALK_CB_NOP);
      walk(userData, ob, (ID **)&surface->init_texture, IDWALK_CB_USER);
      if (surface->effector_weights) {
        walk(userData, ob, (ID **)&surface->effector_weights->group, IDWALK_CB_NOP);
      }
    }
  }
}

static void foreachTexLink(ModifierData *UNUSED(md),
                           Object *UNUSED(ob),
                           TexWalkFunc UNUSED(walk),
                           void *UNUSED(userData))
{
  // walk(userData, ob, md, ""); /* re-enable when possible */
}

ModifierTypeInfo modifierType_DynamicPaint = {
    /* name */ "Dynamic Paint",
    /* structName */ "DynamicPaintModifierData",
    /* structSize */ sizeof(DynamicPaintModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_UsesPointCache | eModifierTypeFlag_Single |
        eModifierTypeFlag_UsesPreview,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ freeRuntimeData,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
