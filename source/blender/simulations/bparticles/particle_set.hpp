#pragma once

#include "particles_container.hpp"

namespace BParticles {

/**
 * A set of particles all of which are in the same block.
 */
struct ParticleSet {
 private:
  AttributeArrays m_attributes;

  /* Indices into the attribute arrays.
   * Invariants:
   *   - Every index must exist at most once.
   *   - The indices must be sorted. */
  ArrayRef<uint> m_pindices;

 public:
  ParticleSet(AttributeArrays attributes, ArrayRef<uint> pindices);

  /**
   * Access the attributes of particles in the block on this set.
   */
  AttributeArrays attributes();

  /**
   * Access particle indices in the block that are part of the set.
   * Every value in this array is an index into the attribute arrays.
   */
  ArrayRef<uint> pindices();

  /**
   * Number of particles in this set.
   */
  uint size() const;

  /**
   * Returns true when pindices()[i] == i for all i, otherwise false.
   */
  bool pindices_are_trivial();

  Range<uint> trivial_pindices();
};

class ParticleSets {
 private:
  AttributesInfo &m_attributes_info;
  Vector<ParticleSet> m_sets;
  uint m_size;

 public:
  ParticleSets(AttributesInfo &attributes_info, ArrayRef<ParticleSet> sets);

  ArrayRef<ParticleSet> sets();

  template<typename T> void set(uint index, ArrayRef<T> data)
  {
    BLI_assert(data.size() == m_size);
    BLI_assert(m_attributes_info.type_of(index) == attribute_type_by_type<T>::value);
    this->set_elements(index, (void *)data.begin());
  }

  template<typename T> void set(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info.attribute_index(name);
    this->set<T>(index, data);
  }

  template<typename T> void set_repeated(uint index, ArrayRef<T> data)
  {
    BLI_assert(m_attributes_info.type_of(index) == attribute_type_by_type<T>::value);
    this->set_repeated_elements(
        index, (void *)data.begin(), data.size(), m_attributes_info.default_value_ptr(index));
  }

  template<typename T> void set_repeated(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info.attribute_index(name);
    this->set_repeated<T>(index, data);
  }

  template<typename T> void fill(uint index, T value)
  {
    BLI_assert(m_attributes_info.type_of(index) == attribute_type_by_type<T>::value);
    this->fill_elements(index, (void *)&value);
  }

  template<typename T> void fill(StringRef name, T value)
  {
    uint index = m_attributes_info.attribute_index(name);
    this->fill<T>(index, value);
  }

  AttributesInfo &attributes_info();

 private:
  void set_elements(uint index, void *data);
  void set_repeated_elements(uint index,
                             void *data,
                             uint data_element_amount,
                             void *default_value);
  void fill_elements(uint index, void *value);
};

/* ParticleSet inline functions
 *******************************************/

inline ParticleSet::ParticleSet(AttributeArrays attributes, ArrayRef<uint> pindices)
    : m_attributes(attributes), m_pindices(pindices)
{
}

inline AttributeArrays ParticleSet::attributes()
{
  return m_attributes;
}

inline ArrayRef<uint> ParticleSet::pindices()
{
  return m_pindices;
}

inline uint ParticleSet::size() const
{
  return m_pindices.size();
}

inline bool ParticleSet::pindices_are_trivial()
{
  if (m_pindices.size() == 0) {
    return true;
  }
  else {
    /* This works due to the invariants mentioned above. */
    return m_pindices.first() == 0 && m_pindices.last() == m_pindices.size() - 1;
  }
}

inline Range<uint> ParticleSet::trivial_pindices()
{
  BLI_assert(this->pindices_are_trivial());
  if (m_pindices.size() == 0) {
    return Range<uint>(0, 0);
  }
  else {
    return Range<uint>(m_pindices.first(), m_pindices.last() + 1);
  }
}

/* ParticleSets inline functions
 ********************************************/

inline ArrayRef<ParticleSet> ParticleSets::sets()
{
  return m_sets;
}

inline AttributesInfo &ParticleSets::attributes_info()
{
  return m_attributes_info;
}

}  // namespace BParticles
