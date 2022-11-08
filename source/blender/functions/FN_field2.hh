/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cpp_type.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender::fn::field2 {

class FieldFunction {
};

class FieldNode {
};

template<typename NodePtr> class GFieldBase {
 protected:
  NodePtr node_ = nullptr;
  int index_ = 0;

  GFieldBase(NodePtr node, const int index) : node_(std::move(node)), index_(index)
  {
  }

 public:
  GFieldBase() = default;

  operator bool() const
  {
    return node_ != nullptr;
  }

  friend bool operator==(const GFieldBase &a, const GFieldBase &b)
  {
    return a.node_ == b.node_ && a.index_ == b.index_;
  }

  uint64_t hash() const
  {
    return get_default_hash_2(node_, index_);
  }

  int index() const
  {
    return index_;
  }

  const FieldNode &node() const
  {
    BLI_assert(*this);
    return *node_;
  }

  const FieldNode *node_ptr() const
  {
    return &*node_;
  }

  const CPPType &cpp_type() const
  {
    /* TODO */
    return CPPType::get<float>();
  }
};

class GField : public GFieldBase<std::shared_ptr<const FieldNode>> {
 public:
  GField() = default;

  GField(std::shared_ptr<const FieldNode> node, const int index = 0)
      : GFieldBase<std::shared_ptr<const FieldNode>>(std::move(node), index)
  {
  }

  template<typename T> Field<T> typed() const;
};

class GFieldRef : public GFieldBase<const FieldNode *> {
 public:
  GFieldRef() = default;

  GFieldRef(const GField &field) : GFieldBase<const FieldNode *>(field.node_ptr(), field.index())
  {
  }

  GFieldRef(const FieldNode &node, const int index = 0)
      : GFieldBase<const FieldNode *>(&node, index)
  {
  }
};

template<typename T> class Field {
 private:
  GField field_;

 public:
  using base_type = T;

  Field() = default;

  Field(std::shared_ptr<const FieldNode> node, const int index = 0)
      : field_(std::move(node), index)
  {
    BLI_assert(!field_ || field_.cpp_type().is<T>());
  }

  operator const GField &() const
  {
    return field_;
  }
};

template<typename T> inline Field<T> GField::typed() const
{
  return Field<T>(this->node_, this->index_);
}

}  // namespace blender::fn::field2
