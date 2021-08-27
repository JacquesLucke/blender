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

#include "NOD_node_declaration.hh"

#include "RNA_types.h"

#include "BLI_float3.hh"

namespace blender::nodes::decl {

class Float : public SocketDeclaration {
 private:
  float default_value_ = 0.0f;
  float min_value_ = -FLT_MAX;
  float max_value_ = FLT_MAX;
  PropertySubType subtype_ = PROP_NONE;

 public:
  Float &min(const float value)
  {
    min_value_ = value;
    return *this;
  }

  Float &max(const float value)
  {
    max_value_ = value;
    return *this;
  }

  Float &default_value(const float value)
  {
    default_value_ = value;
    return *this;
  }

  Float &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Int : public SocketDeclaration {
 private:
  int default_value_ = 0;
  int min_value_ = INT32_MIN;
  int max_value_ = INT32_MAX;
  PropertySubType subtype_ = PROP_NONE;

 public:
  Int &min(const int value)
  {
    min_value_ = value;
    return *this;
  }

  Int &max(const int value)
  {
    max_value_ = value;
    return *this;
  }

  Int &default_value(const int value)
  {
    default_value_ = value;
    return *this;
  }

  Int &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Vector : public SocketDeclaration {
 private:
  float3 default_value_ = {0, 0, 0};
  PropertySubType subtype_ = PROP_NONE;

 public:
  Vector &default_value(const float3 value)
  {
    default_value_ = value;
    return *this;
  }

  Vector &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Object : public SocketDeclaration {
 private:
  bool hide_label_ = false;

 public:
  Object &hide_label(bool value)
  {
    hide_label_ = value;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class Geometry : public SocketDeclaration {
 public:
  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

}  // namespace blender::nodes::decl
