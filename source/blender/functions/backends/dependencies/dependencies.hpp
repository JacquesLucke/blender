#pragma once

#include "FN_core.hpp"

struct Object;

namespace FN {

class ExternalDependenciesBuilder {
 private:
  SmallMultiMap<uint, Object *> m_input_objects;
  SmallMultiMap<uint, Object *> m_output_objects;
  SmallVector<Object *> m_transform_dependencies;

 public:
  ExternalDependenciesBuilder(SmallMultiMap<uint, Object *> inputs) : m_input_objects(inputs)
  {
  }

  void pass_through(uint from_index, uint to_index)
  {
    this->set_output_objects(to_index, this->get_input_objects(from_index));
  }

  void set_output_objects(uint index, ArrayRef<Object *> objects)
  {
    m_output_objects.add_multiple(index, objects);
  }

  ArrayRef<Object *> get_input_objects(uint index)
  {
    return m_input_objects.lookup_default(index);
  }

  ArrayRef<Object *> get_output_objects(uint index)
  {
    return m_output_objects.lookup_default(index);
  }

  void depends_on_transforms_of(ArrayRef<Object *> objects)
  {
    m_transform_dependencies.extend(objects);
  }

  ArrayRef<Object *> get_transform_dependencies()
  {
    return m_transform_dependencies;
  }
};

class DependenciesBody : public FunctionBody {
 public:
  BLI_COMPOSITION_DECLARATION(DependenciesBody);

  virtual ~DependenciesBody()
  {
  }
  virtual void dependencies(ExternalDependenciesBuilder &deps) const = 0;
};

} /* namespace FN */
