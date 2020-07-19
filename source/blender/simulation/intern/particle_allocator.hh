/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SIM_PARTICLE_ALLOCATOR_HH__
#define __SIM_PARTICLE_ALLOCATOR_HH__

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "FN_attributes_ref.hh"

#include <atomic>
#include <mutex>

namespace blender::sim {

class AttributesAllocator : NonCopyable, NonMovable {
 private:
  struct AttributesBlock {
    Array<void *> buffers;
    uint size;
  };

  const fn::AttributesInfo &attributes_info_;
  Vector<std::unique_ptr<AttributesBlock>> allocated_blocks_;
  Vector<fn::MutableAttributesRef> allocated_attributes_;
  uint total_allocated_ = 0;
  std::mutex mutex_;

 public:
  AttributesAllocator(const fn::AttributesInfo &attributes_info)
      : attributes_info_(attributes_info)
  {
  }

  ~AttributesAllocator();

  Span<fn::MutableAttributesRef> get_allocations() const
  {
    return allocated_attributes_;
  }

  uint total_allocated() const
  {
    return total_allocated_;
  }

  const fn::AttributesInfo &attributes_info() const
  {
    return attributes_info_;
  }

  fn::MutableAttributesRef allocate_uninitialized(uint size);
};

class ParticleAllocator : NonCopyable, NonMovable {
 private:
  AttributesAllocator attributes_allocator_;
  std::atomic<uint> next_id_;

 public:
  ParticleAllocator(const fn::AttributesInfo &attributes_info, uint next_id)
      : attributes_allocator_(attributes_info), next_id_(next_id)
  {
  }

  fn::MutableAttributesRef allocate(uint size)
  {
    const fn::AttributesInfo &info = attributes_allocator_.attributes_info();
    fn::MutableAttributesRef attributes = attributes_allocator_.allocate_uninitialized(size);
    for (uint i : info.index_range()) {
      const fn::CPPType &type = info.type_of(i);
      StringRef name = info.name_of(i);
      if (name == "ID") {
        uint start_id = next_id_.fetch_add(size);
        MutableSpan<int> ids = attributes.get<int>("ID");
        for (uint pindex : IndexRange(size)) {
          ids[pindex] = start_id + pindex;
        }
      }
      else {
        type.fill_uninitialized(info.default_of(i), attributes.get(i).buffer(), size);
      }
    }
    return attributes;
  }
};

}  // namespace blender::sim

#endif /* __SIM_PARTICLE_ALLOCATOR_HH__ */
