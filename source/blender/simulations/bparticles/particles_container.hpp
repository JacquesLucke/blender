#pragma once

#include "BLI_array_ref.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_math.hpp"
#include "BLI_string_ref.hpp"

#include "core.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::SmallSet;
using BLI::SmallSetVector;
using BLI::SmallVector;
using BLI::StringRef;
using BLI::Vec3;

class ParticlesContainer;
class ParticlesBlock;
class ParticlesBlockSlice;

class ParticlesContainer {
 private:
  uint m_block_size;
  SmallSetVector<std::string> m_float_attribute_names;
  SmallSetVector<std::string> m_vec3_attribute_names;
  SmallSet<ParticlesBlock *> m_blocks;

 public:
  ParticlesContainer(uint block_size,
                     ArrayRef<std::string> float_attribute_names,
                     ArrayRef<std::string> vec3_attribute_names);

  ~ParticlesContainer();

  uint block_size() const;
  SmallSetVector<std::string> &float_attribute_names();
  SmallSetVector<std::string> &vec3_attribute_names();
  uint float_attribute_amount() const;
  uint vec3_attribute_amount() const;
  uint float_buffer_index(StringRef name) const;
  uint vec3_buffer_index(StringRef name) const;
  const SmallSet<ParticlesBlock *> &active_blocks();

  ParticlesBlock *new_block();
  void release_block(ParticlesBlock *block);
};

class ParticlesBlockSlice : public NamedBuffers {
 private:
  ParticlesBlock *m_block;
  uint m_start;
  uint m_length;

 public:
  ParticlesBlockSlice(ParticlesBlock *block, uint start, uint length);

  ParticlesBlock *block();
  uint size() override;
  ArrayRef<float> float_buffer(StringRef name) override;
  ArrayRef<Vec3> vec3_buffer(StringRef name) override;

  ParticlesBlockSlice take_front(uint n);
};

class ParticlesBlock {
  ParticlesContainer &m_container;
  SmallVector<float *> m_float_buffers;
  SmallVector<Vec3 *> m_vec3_buffers;
  uint m_active_amount;

 public:
  ParticlesBlock(ParticlesContainer &container,
                 ArrayRef<float *> float_buffers,
                 ArrayRef<Vec3 *> vec3_buffers,
                 uint active_amount = 0);

  uint &active_amount();
  uint inactive_amount();
  bool is_full();
  bool is_empty();
  uint next_inactive_index();
  uint size();

  ParticlesContainer &container();

  void clear();

  ArrayRef<float *> float_buffers();
  ArrayRef<Vec3 *> vec3_buffers();
  float *float_buffer(StringRef name);
  Vec3 *vec3_buffer(StringRef name);

  ParticlesBlockSlice slice(uint start, uint length);
  ParticlesBlockSlice slice_all();
  ParticlesBlockSlice slice_active();

  void move(uint old_index, uint new_index);

  static void MoveUntilFull(ParticlesBlock *from, ParticlesBlock *to);
  static void Compress(ArrayRef<ParticlesBlock *> blocks);
};

/* Particles Container
 ***********************************************/

inline uint ParticlesContainer::block_size() const
{
  return m_block_size;
}

inline SmallSetVector<std::string> &ParticlesContainer::float_attribute_names()
{
  return m_float_attribute_names;
}

inline SmallSetVector<std::string> &ParticlesContainer::vec3_attribute_names()
{
  return m_vec3_attribute_names;
}

inline uint ParticlesContainer::float_attribute_amount() const
{
  return m_float_attribute_names.size();
}

inline uint ParticlesContainer::vec3_attribute_amount() const
{
  return m_vec3_attribute_names.size();
}

inline uint ParticlesContainer::float_buffer_index(StringRef name) const
{
  return m_float_attribute_names.index(name.to_std_string());
}

inline uint ParticlesContainer::vec3_buffer_index(StringRef name) const
{
  return m_vec3_attribute_names.index(name.to_std_string());
}

inline const SmallSet<ParticlesBlock *> &ParticlesContainer::active_blocks()
{
  return m_blocks;
}

/* Particles Block
 ****************************************/

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

inline ArrayRef<float *> ParticlesBlock::float_buffers()
{
  return m_float_buffers;
}

inline ArrayRef<Vec3 *> ParticlesBlock::vec3_buffers()
{
  return m_vec3_buffers;
}

inline float *ParticlesBlock::float_buffer(StringRef name)
{
  uint index = m_container.float_buffer_index(name);
  return m_float_buffers[index];
}

inline ParticlesBlockSlice ParticlesBlock::slice(uint start, uint length)
{
  return ParticlesBlockSlice{this, start, length};
}

inline ParticlesBlockSlice ParticlesBlock::slice_all()
{
  return this->slice(0, this->size());
}

inline ParticlesBlockSlice ParticlesBlock::slice_active()
{
  return this->slice(0, m_active_amount);
}

inline Vec3 *ParticlesBlock::vec3_buffer(StringRef name)
{
  uint index = m_container.vec3_buffer_index(name);
  return m_vec3_buffers[index];
}

inline void ParticlesBlock::move(uint old_index, uint new_index)
{
  for (float *buffer : m_float_buffers) {
    buffer[new_index] = buffer[old_index];
  }
  for (Vec3 *buffer : m_vec3_buffers) {
    buffer[new_index] = buffer[old_index];
  }
}

/* Particles Block Slice
 ************************************************/

inline ParticlesBlockSlice::ParticlesBlockSlice(ParticlesBlock *block, uint start, uint length)
    : m_block(block), m_start(start), m_length(length)
{
  BLI_assert(start + length <= block->size());
}

inline ParticlesBlock *ParticlesBlockSlice::block()
{
  return m_block;
}

inline uint ParticlesBlockSlice::size()
{
  return m_length;
}

inline ArrayRef<float> ParticlesBlockSlice::float_buffer(StringRef name)
{
  return ArrayRef<float>(m_block->float_buffer(name) + m_start, m_length);
}

inline ArrayRef<Vec3> ParticlesBlockSlice::vec3_buffer(StringRef name)
{
  return ArrayRef<Vec3>(m_block->vec3_buffer(name) + m_start, m_length);
}

inline ParticlesBlockSlice ParticlesBlockSlice::take_front(uint n)
{
  return ParticlesBlockSlice(m_block, m_start, n);
}

}  // namespace BParticles
