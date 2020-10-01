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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_lib_query.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_volume.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_volume_types.h"

#include "DEG_depsgraph_build.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLI_float4x4.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"

#include "RE_shader_ext.h"

#include "RNA_access.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Interpolation.h>
#  include <openvdb/tools/Morphology.h>
#  include <openvdb/tools/ValueTransformer.h>
#endif

static void initData(ModifierData *md)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  vdmd->texture = NULL;
  vdmd->strength = 1.0f;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  if (vdmd->texture != NULL) {
    DEG_add_generic_id_relation(ctx->node, &vdmd->texture->id, "Volume Displace Modifier");
  }
}

static void foreachObjectLink(ModifierData *md,
                              Object *UNUSED(ob),
                              ObjectWalkFunc UNUSED(walk),
                              void *UNUSED(userData))
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  UNUSED_VARS(vdmd);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  walk(userData, ob, (ID **)&vdmd->texture, IDWALK_CB_USER);
  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
  walk(userData, ob, md, "texture");
}

static bool dependsOnTime(ModifierData *md)
{
  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);
  if (vdmd->texture) {
    return BKE_texture_dependsOnTime(vdmd->texture);
  }
  return false;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  VolumeDisplaceModifierData *vdmd = static_cast<VolumeDisplaceModifierData *>(ptr->data);
  UNUSED_VARS(vdmd);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiTemplateID(layout, C, ptr, "texture", "texture.new", NULL, NULL, 0, ICON_NONE, NULL);

  uiItemR(layout, ptr, "strength", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_VolumeDisplace, panel_draw);
}

#ifdef WITH_OPENVDB
struct DisplaceOp {
  /* Has to be copied for each thread. */
  openvdb::FloatGrid::ConstAccessor accessor;
  Tex *texture;
  const float strength;

  void operator()(const openvdb::FloatGrid::ValueOnIter &iter) const
  {
    const openvdb::Coord coord = iter.getCoord();
    const openvdb::Vec3f coord_object_space = coord.asVec3d() / 100.0;

    openvdb::Vec3d offset{0, 0, 1};
    if (this->texture != NULL) {
      TexResult texture_result = {0};
      BKE_texture_get_value(NULL,
                            this->texture,
                            const_cast<float *>(coord_object_space.asV()),
                            &texture_result,
                            false);
      offset = {texture_result.tr, texture_result.tg, texture_result.tb};
      offset -= 0.5;
    }

    const openvdb::Vec3d sample_coord = coord.asVec3d() + offset * strength;
    const float new_value = openvdb::tools::BoxSampler::sample(this->accessor, sample_coord);
    iter.setValue(new_value);
  }
};
#endif

static Volume *modifyVolume(ModifierData *md,
                            const ModifierEvalContext *UNUSED(ctx),
                            Volume *volume)
{
#ifdef WITH_OPENVDB
  using namespace blender;

  VolumeDisplaceModifierData *vdmd = reinterpret_cast<VolumeDisplaceModifierData *>(md);

  VolumeGrid *volume_grid = BKE_volume_grid_find(volume, "density");
  if (volume_grid == NULL) {
    return volume;
  }
  const VolumeGridType grid_type = BKE_volume_grid_type(volume_grid);
  UNUSED_VARS(grid_type);

  openvdb::FloatGrid::Ptr old_grid = BKE_volume_grid_openvdb_for_write<openvdb::FloatGrid>(
      volume, volume_grid, false);
  openvdb::FloatGrid::Ptr new_grid = old_grid->deepCopy();

  const float max_displacement = std::abs(vdmd->strength);

  openvdb::tools::dilateActiveValues(new_grid->tree(),
                                     static_cast<int>(std::ceil(max_displacement)),
                                     openvdb::tools::NN_FACE_EDGE,
                                     openvdb::tools::EXPAND_TILES);

  DisplaceOp displace_op{old_grid->getConstAccessor(), vdmd->texture, vdmd->strength};

  openvdb::tools::foreach (
      new_grid->beginValueOn(), displace_op, true, /* Disable sharing of the operator. */ false);

  new_grid->pruneGrid();

  old_grid->clear();
  old_grid->merge(*new_grid);

  return volume;
#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(md, "Compiled without OpenVDB");
  return volume;
#endif
}

ModifierTypeInfo modifierType_VolumeDisplace = {
    /* name */ "Volume Displace",
    /* structName */ "VolumeDisplaceModifierData",
    /* structSize */ sizeof(VolumeDisplaceModifierData),
    /* srna */ &RNA_VolumeDisplaceModifier,
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ static_cast<ModifierTypeFlag>(0),
    /* icon */ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ modifyVolume,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
