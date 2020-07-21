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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"

#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {

OBJParentCollection::OBJParentCollection(Main *bmain, Scene *scene) : bmain_(bmain), scene_(scene)
{
  parent_collection_ = BKE_collection_add(
      bmain_, scene_->master_collection, "OBJ import collection");
}

void OBJParentCollection::add_object_to_parent(OBJRawObject *object_to_add, unique_mesh_ptr mesh)
{
  if (object_to_add->object_name.empty()) {
    object_to_add->object_name = "Untitled";
  }
  std::unique_ptr<Object> b_object{
      BKE_object_add_only_object(bmain_, OB_MESH, object_to_add->object_name.c_str())};
  ;
  b_object->data = BKE_object_obdata_add_from_type(
      bmain_, OB_MESH, object_to_add->object_name.c_str());

  BKE_mesh_nomain_to_mesh(
      mesh.release(), (Mesh *)b_object->data, b_object.get(), &CD_MASK_EVERYTHING, true);

  BKE_collection_object_add(bmain_, parent_collection_, b_object.release());
  id_fake_user_set(&parent_collection_->id);

  DEG_id_tag_update(&parent_collection_->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain_);
}

/**
 * Add the given curve object to the OBJ import collection.
 */
void OBJParentCollection::add_object_to_parent(OBJRawObject *object_to_add, unique_curve_ptr curve)
{
  if (object_to_add->object_name.empty() && !object_to_add->nurbs_element.group.empty()) {
    object_to_add->object_name = object_to_add->nurbs_element.group;
  }
  else {
    object_to_add->object_name = "Untitled";
  }
  std::unique_ptr<Object> b_object{
      BKE_object_add_only_object(bmain_, OB_CURVE, object_to_add->object_name.c_str())};
  b_object->data = curve.release();

  BKE_collection_object_add(bmain_, parent_collection_, b_object.release());

  id_fake_user_set(&parent_collection_->id);
  DEG_id_tag_update(&parent_collection_->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain_);
}

}  // namespace blender::io::obj
