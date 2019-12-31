#pragma once

#include "FN_attributes_ref.h"

namespace BParticles {

using BLI::Array;
using BLI::IndexMask;
using BLI::IndexRange;
using BLI::Vector;
using FN::AttributesInfo;
using FN::AttributesInfoBuilder;
using FN::AttributesRef;
using FN::CPPType;
using FN::MutableAttributesRef;

class ParticleSet : BLI::NonCopyable, BLI::NonMovable {
 private:
  const AttributesInfo *m_attributes_info;
  Array<void *> m_attribute_buffers;
  uint m_size;
  uint m_capacity;
  bool m_own_attributes_info;

 public:
  ParticleSet(const AttributesInfo &attributes_info, bool own_attributes_info);
  ~ParticleSet();

  const AttributesInfo &attributes_info() const
  {
    return *m_attributes_info;
  }

  MutableAttributesRef attributes()
  {
    return MutableAttributesRef(*m_attributes_info, m_attribute_buffers, m_size);
  }

  MutableAttributesRef attributes_all()
  {
    return MutableAttributesRef(*m_attributes_info, m_attribute_buffers, m_capacity);
  }

  uint size() const
  {
    return m_size;
  }

  uint remaining_size() const
  {
    return m_capacity - m_size;
  }

  void increase_size_without_realloc(uint amount)
  {
    BLI_assert(m_size + amount <= m_capacity);
    m_size += amount;
  }

  void reserve(uint amount)
  {
    this->realloc_particle_attributes(std::max(amount, m_capacity));
  }

  void add_particles(ParticleSet &particles);

  void update_attributes(const AttributesInfo *new_attributes_info);
  void destruct_and_reorder(IndexMask indices_to_destruct);

  friend bool operator==(const ParticleSet &a, const ParticleSet &b)
  {
    return &a == &b;
  }

 private:
  void realloc_particle_attributes(uint min_size);
};

}  // namespace BParticles
