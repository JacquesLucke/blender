#pragma once

#include <mutex>

#include "BLI_map.hpp"
#include "BLI_stack.hpp"

#include "attributes.hpp"

namespace BParticles {

using BLI::Map;
using BLI::Set;
using BLI::Stack;

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
  SetVector<ParticlesBlock *> m_active_blocks;
  Stack<ParticlesBlock *> m_cached_blocks;
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
  ArrayRef<ParticlesBlock *> active_blocks();

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

  /**
   * Get a vector containing an attribute value from every particle.
   */
  Vector<float> flatten_attribute_float(StringRef attribute_name);
  Vector<float3> flatten_attribute_float3(StringRef attribute_name);

  friend bool operator==(const ParticlesContainer &a, const ParticlesContainer &b);

 private:
  ParticlesBlock *allocate_block();
  void free_block(ParticlesBlock *block);
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

  /**
   * Get the range of attribute indices that contain active particles.
   * This will always start at 0.
   */
  Range<uint> active_range();

  /**
   * Get the number of active particles in this block.
   * This is also a reference, so it allows changing the number of active particles.
   */
  uint &active_amount();

  /**
   * Get the number of inactive attribute "slots" in this block.
   */
  uint unused_amount();

  /**
   * Return true when all attribute arrays are used entirely, otherwise false.
   */
  bool is_full();

  /**
   * Return true when this block contains no particles currently, otherwise false.
   */
  bool is_empty();

  /**
   * Return the first index that is not used currently.
   * Asserts when the block is full.
   */
  uint first_unused_index();

  /**
   * Return the maximum amount of particles in this block.
   */
  uint capacity();

  /**
   * Get the container that owns this block.
   */
  ParticlesContainer &container();

  /**
   * Set the number of active particles in this block to zero.
   */
  void clear();

  /**
   * Get the attributes of all active particles.
   */
  AttributeArrays attributes();

  /**
   * Get the attribute arrays owned by this block. The arrays might be longer than there are active
   * particles currently.
   */
  AttributeArrays attributes_all();

  /**
   * Get a slice of the attribute arrays.
   */
  AttributeArrays attributes_slice(uint start, uint length);
  AttributeArrays attributes_slice(Range<uint> range);

  /**
   * Get the attributes core owned by this block.
   */
  AttributeArraysCore &attributes_core();

  /**
   * Copy the attributes of one particle to another index in the same block.
   */
  void move(uint old_index, uint new_index);

  /**
   * Move as many particles from the end of `from` to the end of `to` as possible. Either `from` is
   * empty first, or `to` is full. Both blocks have to be owned by the same container.
   */
  static void MoveUntilFull(ParticlesBlock &from, ParticlesBlock &to);

  /**
   * Try to fit all particle data into as few blocks as possible, leaving some empty.
   * Afterwards there will be at most on block the is not full and not empty. Empty blocks are not
   * freed by this function.
   */
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
  for (ParticlesBlock *block : m_active_blocks) {
    count += block->active_amount();
  }
  return count;
}

inline AttributesInfo &ParticlesContainer::attributes_info()
{
  return m_attributes_info;
}

inline ArrayRef<ParticlesBlock *> ParticlesContainer::active_blocks()
{
  return m_active_blocks;
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

inline uint ParticlesBlock::unused_amount()
{
  return this->capacity() - m_active_amount;
}

inline bool ParticlesBlock::is_full()
{
  return m_active_amount == this->capacity();
}

inline bool ParticlesBlock::is_empty()
{
  return m_active_amount == 0;
}

inline uint ParticlesBlock::first_unused_index()
{
  BLI_assert(!this->is_full());
  return m_active_amount;
}

inline uint ParticlesBlock::capacity()
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

inline AttributeArrays ParticlesBlock::attributes_slice(Range<uint> range)
{
  if (range.size() == 0) {
    return this->attributes_slice(0, 0);
  }
  return this->attributes_slice(range.first(), range.size());
}

inline AttributeArrays ParticlesBlock::attributes_slice(uint start, uint length)
{
  return m_attributes_core.slice_all().slice(start, length);
}

inline AttributeArrays ParticlesBlock::attributes_all()
{
  return m_attributes_core.slice_all();
}

inline AttributeArrays ParticlesBlock::attributes()
{
  return this->attributes_slice(0, m_active_amount);
}

inline AttributeArraysCore &ParticlesBlock::attributes_core()
{
  return m_attributes_core;
}

inline void ParticlesBlock::move(uint old_index, uint new_index)
{
  AttributesInfo &attributes = m_container.attributes_info();
  AttributeArrays arrays = this->attributes_all();

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
