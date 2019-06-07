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
class BlockBuffersRef;

class ParticlesContainer {
 private:
  uint m_block_size;
  SmallSetVector<std::string> m_float_attribute_names;
  SmallSetVector<std::string> m_vec3_attribute_names;
  SmallSet<ParticlesBlock *> m_blocks;

 public:
  ParticlesContainer(uint block_size,
                     const SmallVector<std::string> &float_attribute_names,
                     const SmallVector<std::string> &vec3_attribute_names);

  ~ParticlesContainer();

  uint block_size() const;
  uint float_attribute_amount() const;
  uint vec3_attribute_amount() const;
  uint float_buffer_index(StringRef name) const;
  uint vec3_buffer_index(StringRef name) const;
  const SmallSet<ParticlesBlock *> &active_blocks();

  ParticlesBlock *new_block();
  void release_block(ParticlesBlock *block);
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

  void move(uint old_index, uint new_index);

  static void MoveUntilFull(ParticlesBlock *from, ParticlesBlock *to);
  static void Compress(ArrayRef<ParticlesBlock *> blocks);
};

class BlockBuffersRef : public NamedBuffersRef {
 private:
  ParticlesBlock *m_block;

 public:
  BlockBuffersRef(ParticlesBlock *block) : m_block(block)
  {
  }

  ArrayRef<float> float_buffer(StringRef name) override
  {
    return ArrayRef<float>(m_block->float_buffer(name), m_block->active_amount());
  }

  ArrayRef<Vec3> vec3_buffer(StringRef name) override
  {
    return ArrayRef<Vec3>(m_block->vec3_buffer(name), m_block->active_amount());
  }
};

/* Particles Container
 ***********************************************/

inline uint ParticlesContainer::block_size() const
{
  return m_block_size;
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

}  // namespace BParticles
