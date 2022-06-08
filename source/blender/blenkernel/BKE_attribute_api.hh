/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_virtual_array.hh"

#include "BKE_attribute_access.hh"

namespace blender::bke {

class AttributeIDRef;

struct AttributeAccessorFunctions {
  bool (*exists)(const void *owner, const AttributeIDRef &attribute_id);
  std::optional<AttributeMetaData> (*try_get_meta_data)(const void *owner,
                                                        const AttributeIDRef &attribute_id);
  bool (*domain_supported)(const void *owner, eAttrDomain domain);
  int (*domain_size)(const void *owner, eAttrDomain domain);
  bool (*is_builtin)(const void *owner, const AttributeIDRef &attribute_id);
  ReadAttributeLookup (*try_get)(const void *owner, const AttributeIDRef &attribute_id);
  GVArray (*try_adapt_domain)(const void *owner,
                              const GVArray &varray,
                              eAttrDomain from_domain,
                              eAttrDomain to_domain);
  bool (*foreach)(const void *owner,
                  FunctionRef<bool(const AttributeIDRef &, const AttributeMetaData &)>);

  WriteAttributeLookup (*try_get_for_write)(void *owner, const AttributeIDRef &attribute_id);
  bool (*try_delete)(void *owner, const AttributeIDRef &attribute_id);
  bool (*try_create)(void *owner,
                     const AttributeIDRef &attribute_id,
                     eAttrDomain domain,
                     eCustomDataType data_type,
                     const AttributeInit &initializer);
};

class AttributeAccessor {
 protected:
  void *owner_;
  const AttributeAccessorFunctions *fn_;

 public:
  AttributeAccessor(const void *owner, const AttributeAccessorFunctions &fn)
      : owner_(const_cast<void *>(owner)), fn_(&fn)
  {
  }

  bool exists(const AttributeIDRef &attribute_id) const
  {
    return fn_->exists(owner_, attribute_id);
  }

  std::optional<AttributeMetaData> try_get_meta_data(const AttributeIDRef &attribute_id) const
  {
    return fn_->try_get_meta_data(owner_, attribute_id);
  }

  bool domain_supported(const eAttrDomain domain) const
  {
    return fn_->domain_supported(owner_, domain);
  }

  int domain_size(const eAttrDomain domain) const
  {
    return fn_->domain_size(owner_, domain);
  }

  bool is_builtin(const AttributeIDRef &attribute_id) const
  {
    return fn_->is_builtin(owner_, attribute_id);
  }

  ReadAttributeLookup try_get(const AttributeIDRef &attribute_id) const
  {
    return fn_->try_get(owner_, attribute_id);
  }

  GVArray try_adapt_domain(const GVArray &varray,
                           const eAttrDomain from_domain,
                           const eAttrDomain to_domain) const
  {
    return fn_->try_adapt_domain(owner_, varray, from_domain, to_domain);
  }

  bool foreach (
      const FunctionRef<bool(const AttributeIDRef &, const AttributeMetaData &)> callback)
  {
    return fn_->foreach (owner_, callback);
  }
};

class MutableAttributeAccessor : public AttributeAccessor {
 public:
  MutableAttributeAccessor(void *owner, const AttributeAccessorFunctions &fn)
      : AttributeAccessor(owner, fn)
  {
  }

  WriteAttributeLookup try_get_for_write(const AttributeIDRef &attribute_id)
  {
    return fn_->try_get_for_write(owner_, attribute_id);
  }

  bool try_delete(const AttributeIDRef &attribute_id)
  {
    return fn_->try_delete(owner_, attribute_id);
  }

  bool try_create(const AttributeIDRef &attribute_id,
                  const eAttrDomain domain,
                  eCustomDataType data_type,
                  const AttributeInit &initializer)
  {
    return fn_->try_create(owner_, attribute_id, domain, data_type, initializer);
  }
};

}  // namespace blender::bke
