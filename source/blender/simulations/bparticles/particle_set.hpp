#pragma once

#include "particles_container.hpp"

namespace BParticles {

/**
 * A set of particles all of which are in the same block.
 */
struct ParticleSet {
 private:
  ParticlesBlock *m_block;

  /* Indices into the attribute arrays.
   * Invariants:
   *   - Every index must exist at most once.
   *   - The indices must be sorted. */
  ArrayRef<uint> m_pindices;

 public:
  ParticleSet(ParticlesBlock &block, ArrayRef<uint> pindices);

  /**
   * Return the block that contains the particles of this set.
   */
  ParticlesBlock &block();

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
  uint size();

  /**
   * Returns true when pindices()[i] == i for all i, otherwise false.
   */
  bool pindices_are_trivial();

  Range<uint> trivial_pindices();
};

class ParticleSets {
 private:
  std::string m_particle_type_name;
  AttributesInfo &m_attributes_info;
  Vector<ParticleSet> m_sets;
  uint m_size;

 public:
  ParticleSets(StringRef particle_type_name,
               AttributesInfo &attributes_info,
               ArrayRef<ParticleSet> sets);

  ArrayRef<ParticleSet> sets();

  void set_byte(uint index, ArrayRef<uint8_t> data);
  void set_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_float(uint index, ArrayRef<float> data);
  void set_float(StringRef name, ArrayRef<float> data);
  void set_float3(uint index, ArrayRef<float3> data);
  void set_float3(StringRef name, ArrayRef<float3> data);

  void set_repeated_byte(uint index, ArrayRef<uint8_t> data);
  void set_repeated_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_repeated_float(uint index, ArrayRef<float> data);
  void set_repeated_float(StringRef name, ArrayRef<float> data);
  void set_repeated_float3(uint index, ArrayRef<float3> data);
  void set_repeated_float3(StringRef name, ArrayRef<float3> data);

  void fill_byte(uint index, uint8_t value);
  void fill_byte(StringRef name, uint8_t value);
  void fill_float(uint index, float value);
  void fill_float(StringRef name, float value);
  void fill_float3(uint index, float3 value);
  void fill_float3(StringRef name, float3 value);

  StringRefNull particle_type_name();

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

inline ParticleSet::ParticleSet(ParticlesBlock &block, ArrayRef<uint> pindices)
    : m_block(&block), m_pindices(pindices)
{
}

inline ParticlesBlock &ParticleSet::block()
{
  return *m_block;
}

inline AttributeArrays ParticleSet::attributes()
{
  return m_block->attributes();
}

inline ArrayRef<uint> ParticleSet::pindices()
{
  return m_pindices;
}

inline uint ParticleSet::size()
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

inline StringRefNull ParticleSets::particle_type_name()
{
  return m_particle_type_name;
}

inline AttributesInfo &ParticleSets::attributes_info()
{
  return m_attributes_info;
}

}  // namespace BParticles
