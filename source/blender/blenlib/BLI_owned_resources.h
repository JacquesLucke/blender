#ifndef __BLI_OWNED_RESOURCES_H__
#define __BLI_OWNED_RESOURCES_H__

#include "BLI_vector.h"
#include "BLI_utility_mixins.h"
#include "BLI_string_ref.h"

namespace BLI {

class OwnedResources : NonCopyable {
 private:
  struct ResourceData {
    void *data;
    void (*free)(void *data);
    const char *name;
  };

  Vector<ResourceData> m_resources;

 public:
  OwnedResources() = default;

  ~OwnedResources()
  {
    for (int i = m_resources.size() - 1; i >= 0; i--) {
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

    ResourceData data;
    data.name = name;
    data.data = static_cast<void *>(resource.release());
    data.free = [](void *data) {
      T *typed_data = static_cast<T *>(data);
      delete typed_data;
    };
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
        std::cout << "  " << data.data << ": " << data.name << '\n';
      }
    }
  }
};

}  // namespace BLI

#endif /* __BLI_OWNED_RESOURCES_H__ */
