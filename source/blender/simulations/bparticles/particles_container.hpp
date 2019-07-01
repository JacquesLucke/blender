#pragma once

#include <mutex>

#include "BLI_small_map.hpp"

#include "attributes.hpp"

namespace BParticles {

using BLI::SmallMap;
using BLI::SmallSet;

class ParticlesContainer;
class ParticlesBlock;

/**
 * A dynamic data structure that can data for an arbitrary amount of particles. All particles in
 * one container must have the same set of attributes.
 *
 * Particles are not stored in the container directly. Instead the container contains multiple
 * blocks, each of which can contain a fixed amount of particles. The number of blocks can change
 * dynamically.
 */
class ParticlesContainer {
 private:
  AttributesInfo m_attributes_info;
  SmallSet<ParticlesBlock *> m_blocks;
  uint m_block_size;
  std::mutex m_blocks_mutex;

 public:
  ParticlesContainer(AttributesInfo attributes, uint block_size);

  ~ParticlesContainer();

  /**
   * Get the maximum number of particles that can fit into the blocks.
   */
  uint block_size() const;

  /**
   * Get the number of particles in this container. For that it is necessary to iterate over all
   * blocks.
   */
  uint count_active() const;

  /**
   * Get information about the attributes of the particles.
   */
  AttributesInfo &attributes_info();

  /**
   * Change the set of attributes. This can involve many allocations and deallocations of attribute
   * buffers.
   */
  void update_attributes(AttributesInfo new_info);

  /**
   * Get a read-only buffer of all the blocks currently in use.
   */
  const SmallSet<ParticlesBlock *> &active_blocks();

  /**
   * Create a new block in this container. It safe to call this function from separate threads at
   * the same time.
   */
  ParticlesBlock &new_block();

  /**
   * Destroy a block. The block must have been created in this container before. It is safe to call
   * this function separate threads at the same time.
   */
  void release_block(ParticlesBlock &block);

  /**
   * Gather an attribute value from all particles in this container and write them into the given
   * buffer.
   */
  void flatten_attribute_data(StringRef attribute_name, void *dst);

  friend bool operator==(const ParticlesContainer &a, const ParticlesContainer &b);
};

/**
 * A block can hold up to a fixed amount of particles. Every block is owned by exactly one
 * particles container. All active particles are at the beginning of the block.
 */
class ParticlesBlock {
  ParticlesContainer &m_container;
  AttributeArraysCore m_attributes_core;
  uint m_active_amount = 0;

  friend ParticlesContainer;

 public:
  ParticlesBlock(ParticlesContainer &container, AttributeArraysCore &attributes_core);

  Range<uint> active_range();
  uint &active_amount();
  uint inactive_amount();
  bool is_full();
  bool is_empty();
  uint next_inactive_index();
  uint size();

  ParticlesContainer &container();

  void clear();

  AttributeArraysCore &attributes_core();
  AttributeArrays slice(Range<uint> range);
  AttributeArrays slice(uint start, uint length);
  AttributeArrays slice_all();
  AttributeArrays attributes();

  void move(uint old_index, uint new_index);

  static void MoveUntilFull(ParticlesBlock &from, ParticlesBlock &to);
  static void Compress(ArrayRef<ParticlesBlock *> blocks);
};

/* Particles Container
 ***********************************************/

inline uint ParticlesContainer::block_size() const
{
  return m_block_size;
}

inline uint ParticlesContainer::count_active() const
{
  uint count = 0;
  for (ParticlesBlock *block : m_blocks) {
    count += block->active_amount();
  }
  return count;
}

inline AttributesInfo &ParticlesContainer::attributes_info()
{
  return m_attributes_info;
}

inline const SmallSet<ParticlesBlock *> &ParticlesContainer::active_blocks()
{
  return m_blocks;
}

inline bool operator==(const ParticlesContainer &a, const ParticlesContainer &b)
{
  return &a == &b;
}

/* Particles Block
 ****************************************/

inline Range<uint> ParticlesBlock::active_range()
{
  return Range<uint>(0, m_active_amount);
}

inline uint &ParticlesBlock::active_amount()
{
  return m_active_amount;
}

inline uint ParticlesBlock::inactive_amount()
{
  return this->size() - m_active_amount;
}

inline bool ParticlesBlock::is_full()
{
  return m_active_amount == this->size();
}

inline bool ParticlesBlock::is_empty()
{
  return m_active_amount == 0;
}

inline uint ParticlesBlock::next_inactive_index()
{
  return m_active_amount;
}

inline uint ParticlesBlock::size()
{
  return m_container.block_size();
}

inline void ParticlesBlock::clear()
{
  m_active_amount = 0;
}

inline ParticlesContainer &ParticlesBlock::container()
{
  return m_container;
}

inline AttributeArrays ParticlesBlock::slice(Range<uint> range)
{
  if (range.size() == 0) {
    return this->slice(0, 0);
  }
  return this->slice(range.first(), range.size());
}

inline AttributeArrays ParticlesBlock::slice(uint start, uint length)
{
  return m_attributes_core.slice_all().slice(start, length);
}

inline AttributeArrays ParticlesBlock::slice_all()
{
  return m_attributes_core.slice_all();
}

inline AttributeArrays ParticlesBlock::attributes()
{
  return this->slice(0, m_active_amount);
}

inline AttributeArraysCore &ParticlesBlock::attributes_core()
{
  return m_attributes_core;
}

inline void ParticlesBlock::move(uint old_index, uint new_index)
{
  AttributesInfo &attributes = m_container.attributes_info();
  AttributeArrays arrays = this->slice_all();

  for (uint i : attributes.byte_attributes()) {
    auto buffer = arrays.get_byte(i);
    buffer[new_index] = buffer[old_index];
  }
  for (uint i : attributes.float_attributes()) {
    auto buffer = arrays.get_float(i);
    buffer[new_index] = buffer[old_index];
  }
  for (uint i : attributes.float3_attributes()) {
    auto buffer = arrays.get_float3(i);
    buffer[new_index] = buffer[old_index];
  }
}

}  // namespace BParticles
