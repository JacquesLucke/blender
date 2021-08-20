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

#include <typeindex>
#include <typeinfo>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

struct uiLayout;
struct bContext;
struct PointerRNA;
struct bNode;

namespace blender::nodes {

class SocketDecl {
 protected:
  std::string name_;

 public:
  SocketDecl(std::string name) : name_(std::move(name))
  {
  }

  virtual ~SocketDecl();

  virtual void build(bNodeTree *tree, bNode *node, eNodeSocketInOut in_out) = 0;
};

namespace decl {

class Float : public SocketDecl {
 private:
  float default_value_ = 0.0f;
  float min_value_ = -FLT_MAX;
  float max_value_ = FLT_MAX;

 public:
  Float(std::string name) : SocketDecl(std::move(name))
  {
  }

  Float &default_value(float value)
  {
    default_value_ = value;
    return *this;
  }

  Float &range(float min, float max)
  {
    min_value_ = min;
    max_value_ = max;
    return *this;
  }

  Float &min(float value)
  {
    min_value_ = value;
    return *this;
  }

  Float &max(float value)
  {
    max_value_ = value;
    return *this;
  }

  void build(bNodeTree *tree, bNode *node, eNodeSocketInOut in_out) override;
};

class Material : public SocketDecl {
 public:
  Material(std::string name) : SocketDecl(std::move(name))
  {
  }

  void build(bNodeTree *tree, bNode *node, eNodeSocketInOut in_out) override;
};

};  // namespace decl

class NodeBuilder {
 private:
  Vector<std::unique_ptr<SocketDecl>> inputs_;
  Vector<std::unique_ptr<SocketDecl>> outputs_;

 public:
  template<typename DeclType> DeclType &input(StringRef name)
  {
    static_assert(std::is_base_of_v<SocketDecl, DeclType>);
    auto socket_decl = std::make_unique<DeclType>(name);
    DeclType *ptr = socket_decl.get();
    inputs_.append(std::move(socket_decl));
    return *ptr;
  }
  template<typename DeclType> DeclType &output(StringRef name)
  {
    static_assert(std::is_base_of_v<SocketDecl, DeclType>);
    auto socket_decl = std::make_unique<DeclType>(name);
    DeclType *ptr = socket_decl.get();
    outputs_.append(std::move(socket_decl));
    return *ptr;
  }

  /* TODO: Move somewhere else. */
  void rebuild(bNodeTree *tree, bNode *node);
};

class NodeDrawer {
 public:
  uiLayout *layout;
  PointerRNA *ptr;
};

class GeoNodeExecParams;

class NodeType {
 private:
  /** Name of the node type. */
  std::string name_;
  /** E.g. #GEO_NODE_CURVE_REVERSE. */
  int builtin_type_;
  /** E.g. #NODE_CLASS_GEOMETRY. */
  int builtin_category_;

 protected:
  std::type_index storage_type_index_ = typeid(void);

 public:
  NodeType(std::string name, int builtin_type, int builtin_category)
      : name_(std::move(name)), builtin_type_(builtin_type), builtin_category_(builtin_category)
  {
  }

  StringRefNull name() const
  {
    return name_;
  }

  int builtin_type() const
  {
    return builtin_type_;
  }

  int builtin_category() const
  {
    return builtin_category_;
  }

  virtual void init(bNodeTree *UNUSED(tree), bNode *node) const {};
  virtual void copy(bNodeTree *dst_tree, bNode *dst_node, const bNode *src_node) const {};
  virtual void free(bNode *node) const {};

  virtual void build(NodeBuilder &builder) const;

  virtual void draw(NodeDrawer &drawer) const;

  virtual void geometry_exec(GeoNodeExecParams params) const = 0;
};

}  // namespace blender::nodes
