#pragma once

#include "particles_container.hpp"

namespace BParticles {

class ForceInterface {
 private:
  ParticlesBlock &m_block;
  ArrayAllocator &m_array_allocator;
  ArrayRef<float3> m_destination;

 public:
  ForceInterface(ParticlesBlock &block,
                 ArrayAllocator &array_allocator,
                 ArrayRef<float3> destination)
      : m_block(block), m_array_allocator(array_allocator), m_destination(destination)
  {
  }

  ParticlesBlock &block()
  {
    return m_block;
  }

  ArrayAllocator &array_allocator()
  {
    return m_array_allocator;
  }

  ArrayRef<float3> combined_destination()
  {
    return m_destination;
  }
};

}  // namespace BParticles
