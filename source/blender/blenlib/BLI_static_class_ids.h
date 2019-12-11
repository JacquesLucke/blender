#ifndef __BLI_STATIC_CLASS_IDS_H__
#define __BLI_STATIC_CLASS_IDS_H__

#include "BLI_utildefines.h"

namespace BLI {

using class_id_t = uintptr_t;

template<typename T> class_id_t get_class_id();

}  // namespace BLI

#define BLI_CREATE_CLASS_ID_UTIL1(class_name, id) \
  namespace BLI { \
  static char class_id_char##id = 0; \
  static class_id_t class_id##id = (class_id_t)&class_id_char##id; \
  template<> class_id_t get_class_id<class_name>() \
  { \
    return class_id##id; \
  } \
  }

#define BLI_CREATE_CLASS_ID_UTIL2(class_name, id) BLI_CREATE_CLASS_ID_UTIL1(class_name, id)

#define BLI_CREATE_CLASS_ID(class_name) BLI_CREATE_CLASS_ID_UTIL2(class_name, __LINE__)

#endif /* __BLI_STATIC_CLASS_IDS_H__ */
