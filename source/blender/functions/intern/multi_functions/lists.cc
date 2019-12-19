#include "lists.h"

namespace FN {

MF_PackList::MF_PackList(const CPPType &base_type, ArrayRef<bool> input_list_status)
    : m_base_type(base_type), m_input_list_status(input_list_status)
{
  MFSignatureBuilder signature = this->get_builder("Pack List");
  if (m_input_list_status.size() == 0) {
    /* Output just an empty list. */
    signature.vector_output("List", m_base_type);
  }
  else if (this->input_is_list(0)) {
    /* Extend the first incoming list. */
    signature.mutable_vector("List", m_base_type);
    for (uint i = 1; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.vector_input("List", m_base_type);
      }
      else {
        signature.single_input("Value", m_base_type);
      }
    }
  }
  else {
    /* Create a new list and append everything. */
    for (uint i = 0; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.vector_input("List", m_base_type);
      }
      else {
        signature.single_input("Value", m_base_type);
      }
    }
    signature.vector_output("List", m_base_type);
  }
}

void MF_PackList::call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const
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
  MFSignatureBuilder signature = this->get_builder("Get List Element");
  signature.vector_input("List", m_base_type);
  signature.single_input<int>("Index");
  signature.single_input("Fallback", m_base_type);
  signature.single_output("Value", m_base_type);
}

void MF_GetListElement::call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  VirtualListRef<int> indices = params.readonly_single_input<int>(1, "Index");
  GenericVirtualListRef fallbacks = params.readonly_single_input(2, "Fallback");

  GenericMutableArrayRef r_output_values = params.uninitialized_single_output(3, "Value");

  for (uint i : mask.indices()) {
    int index = indices[i];
    if (index >= 0) {
      GenericVirtualListRef list = lists[i];
      if (index < list.size()) {
        m_base_type.copy_to_uninitialized(list[index], r_output_values[i]);
        continue;
      }
    }
    m_base_type.copy_to_uninitialized(fallbacks[i], r_output_values[i]);
  }
}

MF_GetListElements::MF_GetListElements(const CPPType &base_type) : m_base_type(base_type)
{
  MFSignatureBuilder signature = this->get_builder("Get List Elements");
  signature.vector_input("List", m_base_type);
  signature.vector_input<int>("Indices");
  signature.single_input("Fallback", m_base_type);
  signature.vector_output("Values", m_base_type);
}

void MF_GetListElements::call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  VirtualListListRef<int> indices = params.readonly_vector_input<int>(1, "Indices");
  GenericVirtualListRef fallbacks = params.readonly_single_input(2, "Fallback");

  GenericVectorArray &r_output_values = params.vector_output(3, "Values");

  for (uint i : mask.indices()) {
    GenericVirtualListRef list = lists[i];
    VirtualListRef<int> sub_indices = indices[i];
    GenericMutableArrayRef values = r_output_values.allocate_single(i, sub_indices.size());
    for (uint j = 0; j < sub_indices.size(); j++) {
      int index = sub_indices[j];
      if (index >= 0 && index < list.size()) {
        values.copy_in__uninitialized(j, list[index]);
      }
      else {
        values.copy_in__uninitialized(j, fallbacks[i]);
      }
    }
  }
}

MF_ListLength::MF_ListLength(const CPPType &base_type) : m_base_type(base_type)
{
  MFSignatureBuilder signature = this->get_builder("List Length");
  signature.vector_input("List", m_base_type);
  signature.single_output<int>("Length");
}

void MF_ListLength::call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  MutableArrayRef<int> lengths = params.uninitialized_single_output<int>(1, "Length");

  for (uint i : mask.indices()) {
    lengths[i] = lists[i].size();
  }
}

};  // namespace FN
