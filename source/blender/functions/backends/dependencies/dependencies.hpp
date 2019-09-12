#pragma once

#include "FN_core.hpp"

#include "BLI_multi_map.h"

struct ID;
struct Object;

namespace FN {

using BLI::MultiMap;

struct DependencyComponents {
  SetVector<Object *> transform_dependencies;
  SetVector<Object *> geometry_dependencies;
};

class FunctionDepsBuilder {
 private:
  const MultiMap<uint, ID *> &m_input_ids;
  MultiMap<uint, ID *> &m_output_ids;
  DependencyComponents &m_dependency_components;

 public:
  FunctionDepsBuilder(const MultiMap<uint, ID *> &input_ids,
                      MultiMap<uint, ID *> &output_ids,
                      DependencyComponents &dependency_components)
      : m_input_ids(input_ids),
        m_output_ids(output_ids),
        m_dependency_components(dependency_components)
  {
  }

  void pass_ids_through(uint input_index, uint output_index)
  {
    this->add_output_ids(output_index, this->get_input_ids(input_index));
  }

  void add_output_ids(uint output_index, ArrayRef<ID *> ids)
  {
    m_output_ids.add_multiple(output_index, ids);
  }

  void add_output_objects(uint output_index, ArrayRef<Object *> objects)
  {
    this->add_output_ids(output_index, objects.cast<ID *>());
  }

  ArrayRef<ID *> get_input_ids(uint input_index)
  {
    return m_input_ids.lookup_default(input_index);
  }

  ArrayRef<Object *> get_input_objects(uint input_index)
  {
    return this->get_input_ids(input_index).cast<Object *>();
  }

  void add_transform_dependency(ArrayRef<Object *> objects)
  {
    m_dependency_components.transform_dependencies.add_multiple(objects);
  }

  void add_geometry_dependency(ArrayRef<Object *> objects)
  {
    m_dependency_components.geometry_dependencies.add_multiple(objects);
  }

  DependencyComponents &dependency_components()
  {
    return m_dependency_components;
  }
};

class DepsBody : public FunctionBody {
 public:
  static const uint FUNCTION_BODY_ID = 0;

  virtual ~DepsBody()
  {
  }
  virtual void build_deps(FunctionDepsBuilder &deps) const = 0;
};

} /* namespace FN */
