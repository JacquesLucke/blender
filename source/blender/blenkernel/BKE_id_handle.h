#ifndef __BKE_ID_HANDLE_H__
#define __BKE_ID_HANDLE_H__

#include "BLI_utildefines.h"

#include "BLI_map.h"

extern "C" {
struct ID;
struct Object;
struct Image;
}

namespace BKE {

using BLI::Map;

/**
 * This is a weak reference to an ID data-block. It does not contain a pointer to the actual data.
 * It can happen that the IDHandle references data, that does not exist anymore. The handle does
 * not know that.
 */
class IDHandle {
 private:
  uint32_t m_identifier;

 public:
  IDHandle() : m_identifier((uint32_t)-1)
  {
  }

  IDHandle(struct ID *id);

  friend bool operator==(IDHandle a, IDHandle b)
  {
    return a.m_identifier == b.m_identifier;
  }

  friend bool operator!=(IDHandle a, IDHandle b)
  {
    return !(a == b);
  }

  uint32_t internal_identifier() const
  {
    return m_identifier;
  }
};

class ObjectIDHandle : public IDHandle {
 public:
  ObjectIDHandle() : IDHandle()
  {
  }

  ObjectIDHandle(struct Object *object);
};

class ImageIDHandle : public IDHandle {
 public:
  ImageIDHandle() : IDHandle()
  {
  }

  ImageIDHandle(struct Image *image);
};

class IDHandleLookup {
 private:
  Map<IDHandle, ID *> m_handle_to_id_map;

 public:
  void add(ID &id)
  {
    IDHandle handle(&id);
    m_handle_to_id_map.add(handle, &id);
  }

  ID *lookup(IDHandle handle) const
  {
    return m_handle_to_id_map.lookup_default(handle, nullptr);
  }

  struct Object *lookup(ObjectIDHandle handle) const
  {
    return reinterpret_cast<struct Object *>(this->lookup((IDHandle)handle));
  }

  struct Image *lookup(ImageIDHandle handle) const
  {
    return reinterpret_cast<struct Image *>(this->lookup((IDHandle)handle));
  }

  static const IDHandleLookup &Empty();
};

}  // namespace BKE

namespace BLI {
template<> struct DefaultHash<BKE::IDHandle> {
  uint32_t operator()(const BKE::IDHandle &value) const
  {
    return value.internal_identifier();
  }
};
}  // namespace BLI

#endif /* __BKE_ID_HANDLE_H__ */