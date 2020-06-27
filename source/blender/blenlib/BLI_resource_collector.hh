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

#ifndef __BLI_RESOURCE_COLLECTOR_HH__
#define __BLI_RESOURCE_COLLECTOR_HH__

/** \file
 * \ingroup bli
 *
 * A ResourceCollector holds an arbitrary set of resources, that will be destructed and/or freed
 * when the ResourceCollector is destructed. This is useful when some object has to take ownership
 * of other objects, but it does not know the type of those other objects.
 */

#include "BLI_linear_allocator.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

class ResourceCollector : NonCopyable, NonMovable {
 private:
  struct ResourceData {
    void *data;
    void (*free)(void *data);
    const char *debug_name;
  };

  LinearAllocator<> m_allocator;
  Vector<ResourceData> m_resources;

 public:
  ResourceCollector() = default;

  ~ResourceCollector()
  {
    /* Free in reversed order. */
    for (uint i = m_resources.size(); i--;) {
      ResourceData &data = m_resources[i];
      data.free(data.data);
    }
  }

  /**
   * Add another object that will be freed when this container is freed. Objects are freed in
   * reverse order.
   */
  template<typename T> void add(std::unique_ptr<T> resource, const char *name)
  {
    BLI_assert(resource.get() != nullptr);
    this->add(
        resource.release(),
        [](void *data) {
          T *typed_data = reinterpret_cast<T *>(data);
          delete typed_data;
        },
        name);
  }

  template<typename T> void add(destruct_ptr<T> resource, const char *name)
  {
    BLI_assert(resource.get() != nullptr);
    this->add(
        resource.release(),
        [](void *data) {
          T *typed_data = reinterpret_cast<T *>(data);
          typed_data->~T();
        },
        name);
  }

  void *allocate(uint size, uint alignment)
  {
    return m_allocator.allocate(size, alignment);
  }

  LinearAllocator<> &allocator()
  {
    return m_allocator;
  }

  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    T *value = m_allocator.construct<T>(std::forward<Args>(args)...);
    this->add(destruct_ptr<T>(value), name);
    return *value;
  }

  void add(void *userdata, void (*free)(void *), const char *name)
  {
    ResourceData data;
    data.debug_name = name;
    data.data = userdata;
    data.free = free;
    m_resources.append(data);
  }

  void print(StringRef name) const
  {
    if (m_resources.size() == 0) {
      std::cout << "\"" << name << "\" has no resources.\n";
      return;
    }
    else {
      std::cout << "Resources for \"" << name << "\":\n";
      for (const ResourceData &data : m_resources) {
        std::cout << "  " << data.data << ": " << data.debug_name << '\n';
      }
    }
  }
};

}  // namespace blender

#endif /* __BLI_RESOURCE_COLLECTOR_HH__ */
