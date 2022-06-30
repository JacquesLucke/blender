/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_virtual_array.hh"

#include "BKE_attribute_access.hh"

namespace blender::bke {

class AttributeIDRef;

template<typename T> struct AttributeReader {
  VArray<T> varray;
  eAttrDomain domain;

  operator bool() const
  {
    return this->varray;
  }
};

template<typename T> struct AttributeWriter {
  VMutableArray<T> varray;
  eAttrDomain domain;
  std::function<void()> tag_modified_fn;

  operator bool() const
  {
    return this->varray;
  }

  void tag_modified()
  {
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }
};

struct GAttributeReader {
  GVArray varray;
  eAttrDomain domain;

  operator bool() const
  {
    return this->varray;
  }

  template<typename T> AttributeReader<T> typed() const
  {
    return {varray.typed<T>(), domain};
  }
};

struct GAttributeWriter {
  GVMutableArray varray;
  eAttrDomain domain;
  std::function<void()> tag_modified_fn;

  operator bool() const
  {
    return this->varray;
  }

  void tag_modified()
  {
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }

  template<typename T> AttributeWriter<T> typed() const
  {
    return {varray.typed<T>(), domain, tag_modified_fn};
  }
};

struct AttributeAccessorFunctions {
  bool (*contains)(const void *owner, const AttributeIDRef &attribute_id);
  std::optional<AttributeMetaData> (*lookup_meta_data)(const void *owner,
                                                       const AttributeIDRef &attribute_id);
  bool (*domain_supported)(const void *owner, eAttrDomain domain);
  int (*domain_size)(const void *owner, eAttrDomain domain);
  bool (*is_builtin)(const void *owner, const AttributeIDRef &attribute_id);
  GAttributeReader (*lookup)(const void *owner, const AttributeIDRef &attribute_id);
  GVArray (*adapt_domain)(const void *owner,
                          const GVArray &varray,
                          eAttrDomain from_domain,
                          eAttrDomain to_domain);
  bool (*foreach)(const void *owner,
                  FunctionRef<bool(const AttributeIDRef &, const AttributeMetaData &)> fn);

  GAttributeWriter (*lookup_for_write)(void *owner, const AttributeIDRef &attribute_id);
  bool (*remove)(void *owner, const AttributeIDRef &attribute_id);
  bool (*add)(void *owner,
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

  /**
   * \return True, when the attribute is available.
   */
  bool contains(const AttributeIDRef &attribute_id) const
  {
    return fn_->contains(owner_, attribute_id);
  }

  /**
   * \return Information about the attribute if it exists.
   */
  std::optional<AttributeMetaData> lookup_meta_data(const AttributeIDRef &attribute_id) const
  {
    return fn_->lookup_meta_data(owner_, attribute_id);
  }

  /**
   * \return True, when attributes can exist on that domain.
   */
  bool domain_supported(const eAttrDomain domain) const
  {
    return fn_->domain_supported(owner_, domain);
  }

  /**
   * \return Number of elements in the given domain.
   */
  int domain_size(const eAttrDomain domain) const
  {
    return fn_->domain_size(owner_, domain);
  }

  /**
   * \return True, when the attribute has a special meaning for Blender and can't be used for
   * arbitrary things.
   */
  bool is_builtin(const AttributeIDRef &attribute_id) const
  {
    return fn_->is_builtin(owner_, attribute_id);
  }

  /**
   * Get read-only access to an attribute.
   */
  GAttributeReader lookup(const AttributeIDRef &attribute_id) const
  {
    return fn_->lookup(owner_, attribute_id);
  }

  /**
   * Interpolate data from one domain to another.
   * TODO: Should this really be part of this API or a separate thing?
   */
  GVArray adapt_domain(const GVArray &varray,
                       const eAttrDomain from_domain,
                       const eAttrDomain to_domain) const
  {
    return fn_->adapt_domain(owner_, varray, from_domain, to_domain);
  }

  /**
   * Run the provided function for every attribute.
   */
  bool foreach (const FunctionRef<bool(const AttributeIDRef &, const AttributeMetaData &)> fn)
  {
    return fn_->foreach (owner_, fn);
  }
};

class MutableAttributeAccessor : public AttributeAccessor {
 public:
  MutableAttributeAccessor(void *owner, const AttributeAccessorFunctions &fn)
      : AttributeAccessor(owner, fn)
  {
  }

  /**
   * Return a writable attribute or none if it does not exist.
   * Make to call #tag_modified after changes are done. This is necessary to potentially invalidate
   * caches.
   */
  GAttributeWriter lookup_for_write(const AttributeIDRef &attribute_id)
  {
    return fn_->lookup_for_write(owner_, attribute_id);
  }

  template<typename T> AttributeWriter<T> lookup_for_write(const AttributeIDRef &attribute_id)
  {
    GAttributeWriter attribute = this->lookup_for_write(attribute_id);
    if (!attribute) {
      return {};
    }
    if (!attribute.varray.type().is<T>()) {
      return {};
    }
    return attribute.typed<T>();
  }

  /**
   * Create a new attribute.
   * \return True, when a new attribute has been created. False, when it's not possible to create
   * this attribute or there is already an attribute with that id.
   */
  bool add(const AttributeIDRef &attribute_id,
           const eAttrDomain domain,
           const eCustomDataType data_type,
           const AttributeInit &initializer)
  {
    return fn_->add(owner_, attribute_id, domain, data_type, initializer);
  }

  /**
   * Find an attribute with the given id, domain and data type. If it does not exist, create a new
   * attribute. If there is an attribute with wrong domain or data type, none is returned.
   */
  GAttributeWriter lookup_or_add_for_write(
      const AttributeIDRef &attribute_id,
      const eAttrDomain domain,
      const eCustomDataType data_type,
      const AttributeInit &initializer = AttributeInitDefault())
  {
    std::optional<AttributeMetaData> meta_data = this->lookup_meta_data(attribute_id);
    if (meta_data.has_value()) {
      if (meta_data->domain == domain && meta_data->data_type == data_type) {
        return this->lookup_for_write(attribute_id);
      }
      return {};
    }
    if (this->add(attribute_id, domain, data_type, initializer)) {
      return this->lookup_for_write(attribute_id);
    }
    return {};
  }

  template<typename T>
  AttributeWriter<T> lookup_or_add_for_write(
      const AttributeIDRef &attribute_id,
      const eAttrDomain domain,
      const AttributeInit &initializer = AttributeInitDefault())
  {
    const CPPType &cpp_type = CPPType::get<T>();
    const eCustomDataType data_type = cpp_type_to_custom_data_type(cpp_type);
    return this->lookup_or_add_for_write(attribute_id, domain, data_type, initializer).typed<T>();
  }

  /**
   * Remove an attribute.
   * \return True, when the attribute has been deleted. False, when it's not possible to delete
   * this attribute or if there is no attribute with that id.
   */
  bool remove(const AttributeIDRef &attribute_id)
  {
    return fn_->remove(owner_, attribute_id);
  }
};

}  // namespace blender::bke
