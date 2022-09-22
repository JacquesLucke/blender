/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_utildefines.h"
#include "DNA_copy_on_write.h"

#ifdef __cplusplus
namespace blender::bke {
class Attribute;
class Attributes;
class AttributeRuntime;
class AttributesRuntime;
}  // namespace blender::bke
#endif

typedef enum AttributeStorageType {
  ATTR_STORAGE_TYPE_SPARSE_INDICES = 0,
  ATTR_STORAGE_TYPE_DENSE_ARRAY = 1,
} AttributeStorageType;

typedef enum AttributeBaseType {
  ATTR_DATA_TYPE_FLOAT = 0,
  ATTR_DATA_TYPE_DOUBLE = 1,
  ATTR_DATA_TYPE_INT8 = 2,
  ATTR_DATA_TYPE_INT16 = 3,
  ATTR_DATA_TYPE_INT32 = 4,
  ATTR_DATA_TYPE_INT64 = 5,
} AttributeDataType;

typedef enum AttributeDomain {
  ATTR_DOMAIN_POINT = 0,
  ATTR_DOMAIN_EDGE = 1,
  ATTR_DOMAIN_FACE = 2,
  ATTR_DOMAIN_CORNER = 3,
  ATTR_DOMAIN_CURVE = 4,
  ATTR_DOMAIN_INSTANCE = 5,
} AttributeDomain;

typedef struct Attribute {
  /** AttributeStorageType. */
  uint8_t storage_type;
  /** AttributeDomain. */
  uint8_t domain;
  /** AttributeBaseType. */
  uint8_t base_type;
  /** Number base type elements per element in the domain. */
  int array_size;
  int domain_size;

  char *name;
  void *runtime;

  /**
   * What's stored in these pointers depends on #storage_type.
   */
  void *values;
  int num_indices;
  int *indices;
  void *fallback;

  bCopyOnWrite *values_cow;
  bCopyOnWrite *indices_cow;

#ifdef __cplusplus
  blender::bke::Attribute &wrap();
  const blender::bke::Attribute &wrap() const;
#endif
} Attribute;

typedef struct Attributes {
  Attribute **attributes;
  int num_attributes;
  int capacity_attributes;

  /** #AttributesRuntime. */
  void *runtime;

#ifdef __cplusplus
  blender::bke::Attributes &wrap();
  const blender::bke::Attributes &wrap() const;
#endif
} Attributes;
