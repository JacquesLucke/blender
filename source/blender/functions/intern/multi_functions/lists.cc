#include "lists.h"

namespace FN {

MF_PackList::MF_PackList(const CPPType &base_type, ArrayRef<bool> input_list_status)
    : m_base_type(base_type), m_input_list_status(input_list_status)
{
  MFSignatureBuilder signature("Pack List");
  if (m_input_list_status.size() == 0) {
    /* Output just an empty list. */
    signature.vector_output("List", m_base_type);
  }
  else if (this->input_is_list(0)) {
    /* Extend the first incoming list. */
    signature.mutable_vector("List", m_base_type);
    for (uint i = 1; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.readonly_vector_input("List", m_base_type);
      }
      else {
        signature.readonly_single_input("Value", m_base_type);
      }
    }
  }
  else {
    /* Create a new list and append everything. */
    for (uint i = 0; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.readonly_vector_input("List", m_base_type);
      }
      else {
        signature.readonly_single_input("Value", m_base_type);
      }
    }
    signature.vector_output("List", m_base_type);
  }
  this->set_signature(signature);
}

void MF_PackList::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVectorArray *vector_array;
  bool is_mutating_first_list;
  if (m_input_list_status.size() == 0) {
    vector_array = &params.vector_output(0, "List");
    is_mutating_first_list = false;
  }
  else if (this->input_is_list(0)) {
    vector_array = &params.mutable_vector(0, "List");
    is_mutating_first_list = true;
  }
  else {
    vector_array = &params.vector_output(m_input_list_status.size(), "List");
    is_mutating_first_list = false;
  }

  uint first_index = is_mutating_first_list ? 1 : 0;
  for (uint input_index = first_index; input_index < m_input_list_status.size(); input_index++) {
    if (this->input_is_list(input_index)) {
      GenericVirtualListListRef list = params.readonly_vector_input(input_index, "List");
      for (uint i : mask.indices()) {
        vector_array->extend_single__copy(i, list[i]);
      }
    }
    else {
      GenericVirtualListRef list = params.readonly_single_input(input_index, "Value");
      for (uint i : mask.indices()) {
        vector_array->append_single__copy(i, list[i]);
      }
    }
  }
}

bool MF_PackList::input_is_list(uint index) const
{
  return m_input_list_status[index];
}

MF_GetListElement::MF_GetListElement(const CPPType &base_type) : m_base_type(base_type)
{
  MFSignatureBuilder signature("Get List Element");
  signature.readonly_vector_input("List", m_base_type);
  signature.readonly_single_input<int>("Index");
  signature.readonly_single_input("Fallback", m_base_type);
  signature.single_output("Value", m_base_type);
  this->set_signature(signature);
}

void MF_GetListElement::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  VirtualListRef<int> indices = params.readonly_single_input<int>(1, "Index");
  GenericVirtualListRef fallbacks = params.readonly_single_input(2, "Fallback");

  GenericMutableArrayRef output_values = params.uninitialized_single_output(3, "Value");

  for (uint i : mask.indices()) {
    int index = indices[i];
    if (index >= 0) {
      GenericVirtualListRef list = lists[i];
      if (index < list.size()) {
        m_base_type.copy_to_uninitialized(list[index], output_values[i]);
        continue;
      }
    }
    m_base_type.copy_to_uninitialized(fallbacks[i], output_values[i]);
  }
}

MF_ListLength::MF_ListLength(const CPPType &base_type) : m_base_type(base_type)
{
  MFSignatureBuilder signature("List Length");
  signature.readonly_vector_input("List", m_base_type);
  signature.single_output<int>("Length");
  this->set_signature(signature);
}

void MF_ListLength::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  MutableArrayRef<int> lengths = params.uninitialized_single_output<int>(1, "Length");

  for (uint i : mask.indices()) {
    lengths[i] = lists[i].size();
  }
}

};  // namespace FN
