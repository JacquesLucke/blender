#pragma once

#include "FN_core.hpp"

struct ID;
struct Object;

namespace FN {

class FunctionDepsBuilder {
 private:
  const SmallMultiMap<uint, ID *> &m_input_ids;
  SmallMultiMap<uint, ID *> &m_output_ids;
  SmallSetVector<Object *> &m_transform_dependencies;

 public:
  FunctionDepsBuilder(const SmallMultiMap<uint, ID *> &input_ids,
                      SmallMultiMap<uint, ID *> &output_ids,
                      SmallSetVector<Object *> &transform_dependencies)
      : m_input_ids(input_ids),
        m_output_ids(output_ids),
        m_transform_dependencies(transform_dependencies)
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
    m_transform_dependencies.add_multiple(objects);
  }
};

class DepsBody : public FunctionBody {
 public:
  BLI_COMPOSITION_DECLARATION(DepsBody);

  virtual ~DepsBody()
  {
  }
  virtual void build_deps(FunctionDepsBuilder &deps) const = 0;
};

} /* namespace FN */
