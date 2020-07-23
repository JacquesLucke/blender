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

const int OBJRawObject::object_type() const
{
  return object_type_;
}

const std::string &OBJRawObject::object_name() const
{
  return object_name_;
}

Span<int> OBJRawObject::vertex_indices() const
{
  return vertex_indices_;
}

const int64_t OBJRawObject::tot_verts() const
{
  return vertex_indices_.size();
}

Span<OBJFaceElem> OBJRawObject::face_elements() const
{
  return face_elements_;
}

const int64_t OBJRawObject::tot_face_elems() const
{
  return face_elements_.size();
}

Span<int> OBJRawObject::uv_vertex_indices() const
{
  return uv_vertex_indices_;
}

/**
 * Return per-object total UV vertex indices that index into a global list of vertex coordinates.
 */
const int64_t OBJRawObject::tot_uv_vert_indices() const
{
  return uv_vertex_indices_.size();
}

Span<MEdge> OBJRawObject::edges() const
{
  return edges_;
}

const int64_t OBJRawObject::tot_edges() const
{
  return edges_.size();
}

const int OBJRawObject::tot_loops() const
{
  return tot_loops_;
}

const int OBJRawObject::tot_normals() const
{
  return tot_normals_;
}

/**
 * Total UV vertices that an object's faces' corners refer to in "f" lines.
 */
const int OBJRawObject::tot_uv_verts() const
{
  return tot_uv_verts_;
}

const OBJNurbsElem &OBJRawObject::nurbs_elem() const
{
  return nurbs_element_;
}

const std::string &OBJRawObject::group() const
{
  return nurbs_element_.group_;
}

/**
 * Create a collection to store all imported objects.
 */
OBJImportCollection::OBJImportCollection(Main *bmain, Scene *scene) : bmain_(bmain), scene_(scene)
{
  obj_import_collection_ = BKE_collection_add(
      bmain_, scene_->master_collection, "OBJ import collection");
}

/**
 * Add the given Mesh/CurveUniqueObjectDeleter object to the OBJ import collection.
 */
void OBJImportCollection::add_object_to_collection(unique_object_ptr b_object)
{
  BKE_collection_object_add(bmain_, obj_import_collection_, b_object.release());
  id_fake_user_set(&obj_import_collection_->id);
  DEG_id_tag_update(&obj_import_collection_->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain_);
}

}  // namespace blender::io::obj
