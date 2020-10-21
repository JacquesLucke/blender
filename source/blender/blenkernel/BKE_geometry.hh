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

#pragma once

/** \file
 * \ingroup bke
 */

struct Mesh;

namespace blender::bke {

class Geometry {
 private:
  /* Only contains a mesh for now. */
  Mesh *mesh_ = nullptr;

 public:
  Geometry() = default;
  ~Geometry();

  /** Takes ownership of the mesh. */
  static Geometry *from_mesh(Mesh *mesh)
  {
    Geometry *geometry = new Geometry();
    geometry->mesh_ = mesh;
    return geometry;
  }

  Mesh *mesh()
  {
    return mesh_;
  }

  /* The caller takes ownership of the mesh and removes it from the geometry. */
  Mesh *extract_mesh()
  {
    Mesh *mesh = mesh_;
    mesh_ = mesh;
    return mesh;
  }
};

}  // namespace blender::bke
