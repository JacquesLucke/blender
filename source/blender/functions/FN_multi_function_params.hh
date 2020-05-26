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

#ifndef __FN_MULTI_FUNCTION_PARAMS_HH__
#define __FN_MULTI_FUNCTION_PARAMS_HH__

#include "FN_array_refs.hh"
#include "FN_generic_vector_array.hh"
#include "FN_multi_function_signature.hh"
#include "FN_vector_array_refs.hh"

namespace FN {

class MFParamsBuilder {
 private:
  const MFSignatureData *m_signature;
  uint m_min_array_size;
  Vector<GenericVirtualArrayRef> m_virtual_array_refs;
  Vector<GenericMutableArrayRef> m_mutable_array_refs;
  Vector<GenericVirtualVectorArrayRef> m_virtual_vector_array_refs;
  Vector<GenericVectorArray *> m_vector_arrays;

  friend class MFParams;

 public:
  MFParamsBuilder(const MFSignatureData &signature, uint min_array_size)
      : m_signature(&signature), m_min_array_size(min_array_size)
  {
  }

  void add_readonly_single_input(GenericVirtualArrayRef ref)
  {
    this->assert_current_param_type(MFParamType::ForSingleInput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_virtual_array_refs.append(ref);
  }

  void add_readonly_vector_input(GenericVirtualVectorArrayRef ref)
  {
    this->assert_current_param_type(MFParamType::ForVectorInput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_virtual_vector_array_refs.append(ref);
  }

  void add_single_output(GenericMutableArrayRef ref)
  {
    this->assert_current_param_type(MFParamType::ForSingleOutput(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_mutable_array_refs.append(ref);
  }

  void add_vector_output(GenericVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForVectorOutput(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  void add_single_mutable(GenericMutableArrayRef ref)
  {
    this->assert_current_param_type(MFParamType::ForMutableSingle(ref.type()));
    BLI_assert(ref.size() >= m_min_array_size);
    m_mutable_array_refs.append(ref);
  }

  void add_vector_mutable(GenericVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForMutableVector(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  GenericMutableArrayRef computed_array(uint param_index)
  {
    BLI_assert(ELEM(m_signature->param_types[param_index].category(),
                    MFParamType::SingleOutput,
                    MFParamType::SingleMutable));
    uint data_index = m_signature->data_index(param_index);
    return m_mutable_array_refs[data_index];
  }

  GenericVectorArray &computed_vector_array(uint param_index)
  {
    BLI_assert(ELEM(m_signature->param_types[param_index].category(),
                    MFParamType::VectorOutput,
                    MFParamType::VectorMutable));
    uint data_index = m_signature->data_index(param_index);
    return *m_vector_arrays[data_index];
  }

 private:
  void assert_current_param_type(MFParamType param_type)
  {
    UNUSED_VARS_NDEBUG(param_type);
#ifdef DEBUG
    uint param_index = this->current_param_index();
    MFParamType expected_type = m_signature->param_types[param_index];
    BLI_assert(expected_type == param_type);
#endif
  }

  uint current_param_index() const
  {
    return m_virtual_array_refs.size() + m_mutable_array_refs.size() +
           m_virtual_vector_array_refs.size() + m_vector_arrays.size();
  }
};

class MFParams {
 private:
  MFParamsBuilder *m_builder;

 public:
  MFParams(MFParamsBuilder &builder) : m_builder(&builder)
  {
  }

  GenericVirtualArrayRef readonly_single_input(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleInput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_virtual_array_refs[data_index];
  }

  GenericMutableArrayRef uninitialized_single_output(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleOutput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_mutable_array_refs[data_index];
  }

  GenericVirtualVectorArrayRef readonly_vector_input(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorInput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_virtual_vector_array_refs[data_index];
  }

  GenericVectorArray &vector_output(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorOutput);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return *m_builder->m_vector_arrays[data_index];
  }

  GenericMutableArrayRef single_mutable(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::SingleMutable);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return m_builder->m_mutable_array_refs[data_index];
  }

  GenericVectorArray &vector_mutable(uint param_index, StringRef name = "")
  {
    this->assert_correct_param(param_index, name, MFParamType::VectorMutable);
    uint data_index = m_builder->m_signature->data_index(param_index);
    return *m_builder->m_vector_arrays[data_index];
  }

 private:
  void assert_correct_param(uint param_index, StringRef name, MFParamType param_type)
  {
    UNUSED_VARS_NDEBUG(param_index, name, param_type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[param_index] == param_type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[param_index] == name);
    }
#endif
  }

  void assert_correct_param(uint param_index, StringRef name, MFParamType::Category category)
  {
    UNUSED_VARS_NDEBUG(param_index, name, category);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[param_index].category() == category);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[param_index] == name);
    }
#endif
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_PARAMS_HH__ */
