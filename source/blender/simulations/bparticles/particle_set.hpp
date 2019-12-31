#pragma once

#include "FN_attributes_ref.h"

namespace BParticles {

using BLI::IndexMask;
using BLI::Vector;
using FN::AttributesInfo;
using FN::AttributesInfoBuilder;
using FN::AttributesRef;

class ParticleSet {
 private:
  std::unique_ptr<AttributesInfo> m_attributes_info;
  Vector<void *> m_attribute_buffers;
  uint m_size;
  uint m_capacity;

 public:
  ParticleSet(const AttributesInfoBuilder &attributes_info_builder, uint size);
  ~ParticleSet();

  const AttributesInfo &attributes_info() const
  {
    return *m_attributes_info;
  }

  AttributesRef attributes()
  {
    return AttributesRef(*m_attributes_info, m_attribute_buffers, m_size);
  }

  uint size() const
  {
    return m_size;
  }

  void add_particles(ParticleSet &particles);

  void update_attributes(const AttributesInfoBuilder &new_attributes_info_builder);
  void destruct_and_reorder(IndexMask indices_to_destruct);

  friend bool operator==(const ParticleSet &a, const ParticleSet &b)
  {
    return &a == &b;
  }
};

}  // namespace BParticles
