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

#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn {

MFProcedureExecutor::MFProcedureExecutor(std::string name, const MFProcedure &procedure)
    : procedure_(procedure)
{
  MFSignatureBuilder signature(std::move(name));

  for (const std::pair<MFParamType::InterfaceType, const MFVariable *> &param :
       procedure.params()) {
    signature.add(param.second->name(), MFParamType(param.first, param.second->data_type()));
  }

  signature_ = signature.build();
  this->set_signature(&signature_);
}

using IndicesSplitVectors = std::array<Vector<int64_t>, 2>;

class VariableStore : NonCopyable, NonMovable {
 public:
  VariableStore() = default;
  virtual ~VariableStore() = default;

  virtual void load_as_input(MFParamsBuilder &params) = 0;

  virtual void load_as_mutable(MFParamsBuilder &params, IndexMask full_mask)
  {
    /* Should be overridden by the classes that support it. */
    BLI_assert_unreachable();
    UNUSED_VARS(params, full_mask);
  }

  virtual void load_as_output(MFParamsBuilder &params, IndexMask mask, IndexMask full_mask)
  {
    /* Should be overridden by the classes that support it. */
    BLI_assert_unreachable();
    UNUSED_VARS(params, mask, full_mask);
  }

  virtual void destruct(IndexMask mask)
  {
    /* Do nothing in the general case.  */
    UNUSED_VARS(mask);
  }

  virtual void indices_split(IndexMask mask, IndicesSplitVectors &r_indices)
  {
    /* Should be overridden by the classes that support it. */
    BLI_assert_unreachable();
    UNUSED_VARS(mask, r_indices);
  }
};

class VariableStore_VirtualSingleFromCaller : public VariableStore {
 private:
  const GVArray &data_;

 public:
  VariableStore_VirtualSingleFromCaller(const GVArray &data) : data_(data)
  {
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_single_input(data_);
  }

  void indices_split(IndexMask mask, IndicesSplitVectors &r_indices) final
  {
    BLI_assert(data_.type().is<bool>());
    for (const int i : mask) {
      bool condition;
      data_.get(i, &condition);
      r_indices[condition].append(i);
    }
  }
};

class VariableStore_SingleFromCaller : public VariableStore {
 private:
  GMutableSpan data_;

 public:
  VariableStore_SingleFromCaller(GMutableSpan data) : data_(data)
  {
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_single_input(data_);
  }

  void load_as_mutable(MFParamsBuilder &params, IndexMask UNUSED(full_mask)) final
  {
    params.add_single_mutable(data_);
  }

  void load_as_output(MFParamsBuilder &params,
                      IndexMask UNUSED(mask),
                      IndexMask UNUSED(full_mask)) final
  {
    params.add_uninitialized_single_output(data_);
  }

  void destruct(IndexMask mask) final
  {
    const CPPType &type = data_.type();
    type.destruct_indices(data_.data(), mask);
  }

  void indices_split(IndexMask mask, IndicesSplitVectors &r_indices) final
  {
    Span<bool> conditions = data_.typed<bool>();
    for (const int i : mask) {
      r_indices[conditions[i]].append(i);
    }
  }
};

class VariableStore_VirtualVectorFromCaller : public VariableStore {
 private:
  const GVVectorArray &data_;

 public:
  VariableStore_VirtualVectorFromCaller(const GVVectorArray &data) : data_(data)
  {
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_vector_input(data_);
  }
};

class VariableStore_VectorFromCaller : public VariableStore {
 private:
  GVectorArray &data_;

 public:
  VariableStore_VectorFromCaller(GVectorArray &data) : data_(data)
  {
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_vector_input(data_);
  }

  void load_as_mutable(MFParamsBuilder &params, IndexMask UNUSED(full_mask)) final
  {
    params.add_vector_mutable(data_);
  }

  void load_as_output(MFParamsBuilder &params,
                      IndexMask UNUSED(mask),
                      IndexMask UNUSED(full_mask)) final
  {
    params.add_vector_output(data_);
  }

  void destruct(IndexMask mask) final
  {
    data_.clear(mask);
  }
};

class VariableStore_SingleOwn : public VariableStore {
 private:
  GMutableSpan data_;
  int64_t tot_initialized_ = 0;

 public:
  VariableStore_SingleOwn(const CPPType &type) : data_(type)
  {
  }

  ~VariableStore_SingleOwn() final
  {
    void *buffer = data_.data();
    if (buffer != nullptr) {
      MEM_freeN(buffer);
    }
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_single_input(data_);
  }

  void load_as_mutable(MFParamsBuilder &params, IndexMask full_mask) final
  {
    this->ensure_array_buffer(full_mask);
    params.add_single_mutable(data_);
  }

  void load_as_output(MFParamsBuilder &params, IndexMask mask, IndexMask full_mask) final
  {
    this->ensure_array_buffer(full_mask);
    tot_initialized_ += mask.size();
    params.add_uninitialized_single_output(data_);
  }

  void destruct(IndexMask mask) final
  {
    const CPPType &type = data_.type();
    type.destruct_indices(data_.data(), mask);
    tot_initialized_ -= mask.size();

    if (tot_initialized_ == 0) {
      void *buffer = data_.data();
      if (buffer != nullptr) {
        MEM_freeN(buffer);
        data_ = GMutableSpan{data_.type()};
      }
    }
  }

  void indices_split(IndexMask mask, IndicesSplitVectors &r_indices) final
  {
    Span<bool> conditions = data_.typed<bool>();
    for (const int i : mask) {
      r_indices[conditions[i]].append(i);
    }
  }

 private:
  void ensure_array_buffer(IndexMask full_mask)
  {
    const int min_array_size = full_mask.min_array_size();
    if (data_.size() < min_array_size) {
      BLI_assert(data_.is_empty());
      const CPPType &type = data_.type();
      void *buffer = MEM_mallocN_aligned(type.size() * min_array_size, type.alignment(), __func__);
      data_ = GMutableSpan{type, buffer, min_array_size};
    }
  }
};

class VariableStore_VectorOwn : public VariableStore {
 private:
  const CPPType &type_;
  std::unique_ptr<GVectorArray> data_;
  int64_t tot_initialized_ = 0;

 public:
  VariableStore_VectorOwn(const CPPType &type) : type_(type)
  {
  }

  void load_as_input(MFParamsBuilder &params) final
  {
    params.add_readonly_vector_input(*data_);
  }

  void load_as_mutable(MFParamsBuilder &params, IndexMask full_mask) final
  {
    this->ensure_array_buffer(full_mask);
    params.add_vector_mutable(*data_);
  }

  void load_as_output(MFParamsBuilder &params, IndexMask mask, IndexMask full_mask) final
  {
    this->ensure_array_buffer(full_mask);
    tot_initialized_ += mask.size();
    params.add_vector_output(*data_);
  }

  void destruct(IndexMask mask) final
  {
    data_->clear(mask);
    tot_initialized_ -= mask.size();

    if (tot_initialized_ == 0) {
      data_.reset();
    }
  }

 private:
  void ensure_array_buffer(IndexMask full_mask)
  {
    if (!data_) {
      data_ = std::make_unique<GVectorArray>(type_, full_mask.min_array_size());
    }
  }
};

class VariableStoreContainer {
 private:
  LinearAllocator<> allocator_;
  Map<const MFVariable *, destruct_ptr<VariableStore>> stores_;
  IndexMask full_mask_;

 public:
  VariableStoreContainer(const MFProcedureExecutor &fn,
                         const MFProcedure &procedure,
                         IndexMask mask,
                         MFParams &params)
      : full_mask_(mask)
  {
    for (const int param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      const MFVariable *variable = procedure.params()[param_index].second;
      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &data = params.readonly_single_input(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_VirtualSingleFromCaller>(data));
          break;
        }
        case MFParamType::VectorInput: {
          const GVVectorArray &data = params.readonly_vector_input(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_VirtualVectorFromCaller>(data));
          break;
        }
        case MFParamType::SingleOutput: {
          GMutableSpan data = params.uninitialized_single_output(param_index);
          stores_.add_new(variable, allocator_.construct<VariableStore_SingleFromCaller>(data));
          break;
        }
        case MFParamType::VectorOutput: {
          GVectorArray &data = params.vector_output(param_index);
          stores_.add_new(variable, allocator_.construct<VariableStore_VectorFromCaller>(data));
          break;
        }
        case MFParamType::SingleMutable: {
          GMutableSpan data = params.single_mutable(param_index);
          stores_.add_new(variable, allocator_.construct<VariableStore_SingleFromCaller>(data));
          break;
        }
        case MFParamType::VectorMutable: {
          GVectorArray &data = params.vector_mutable(param_index);
          stores_.add_new(variable, allocator_.construct<VariableStore_VectorFromCaller>(data));
          break;
        }
      }
    }
  }

  void load_param(MFParamsBuilder &params,
                  const MFVariable &variable,
                  const MFParamType &param_type,
                  const IndexMask &mask)
  {
    VariableStore &store = this->get_store_for_variable(variable);
    switch (param_type.interface_type()) {
      case MFParamType::Input: {
        store.load_as_input(params);
        break;
      }
      case MFParamType::Mutable: {
        store.load_as_mutable(params, full_mask_);
        break;
      }
      case MFParamType::Output: {
        store.load_as_output(params, mask, full_mask_);
        break;
      }
    }
  }

  void destruct(const MFVariable &variable, const IndexMask &mask)
  {
    destruct_ptr<VariableStore> *store_ptr = stores_.lookup_ptr(&variable);
    if (store_ptr != nullptr) {
      store_ptr->get()->destruct(mask);
    }
  }

  VariableStore &get_store_for_variable(const MFVariable &variable)
  {
    return *stores_.lookup_or_add_cb(
        &variable, [&]() { return this->create_store_for_own_variable(variable); });
  }

  destruct_ptr<VariableStore> create_store_for_own_variable(const MFVariable &variable)
  {
    MFDataType data_type = variable.data_type();
    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single_type();
        return allocator_.construct<VariableStore_SingleOwn>(type);
      }
      case MFDataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        return allocator_.construct<VariableStore_VectorOwn>(type);
      }
    }
    BLI_assert_unreachable();
    return nullptr;
  }
};

static void execute_call_instruction(const MFCallInstruction &instruction,
                                     IndexMask mask,
                                     VariableStoreContainer &variable_stores,
                                     const MFContext &context)
{
  const MultiFunction &fn = instruction.fn();
  MFParamsBuilder params(fn, mask.min_array_size());

  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    const MFVariable *variable = instruction.params()[param_index];
    variable_stores.load_param(params, *variable, param_type, mask);
  }

  fn.call(mask, params, context);
}

struct NextInstructionInfo {
  const MFInstruction *instruction = nullptr;
  Vector<int64_t> owned_indices;

  IndexMask mask() const
  {
    return this->owned_indices.as_span();
  }

  operator bool() const
  {
    return this->instruction != nullptr;
  }
};

class InstructionScheduler {
 private:
  Map<const MFInstruction *, Vector<Vector<int64_t>>> indices_by_instruction_;

 public:
  InstructionScheduler() = default;

  void add_referenced_indices(const MFInstruction *instruction, IndexMask mask)
  {
    if (instruction == nullptr) {
      return;
    }
    if (mask.is_empty()) {
      return;
    }
    indices_by_instruction_.lookup_or_add_default(instruction).append(mask.indices());
  }

  void add_owned_indices(const MFInstruction *instruction, Vector<int64_t> indices)
  {
    if (instruction == nullptr) {
      return;
    }
    if (indices.is_empty()) {
      return;
    }
    BLI_assert(IndexMask::indices_are_valid_index_mask(indices));
    indices_by_instruction_.lookup_or_add_default(instruction).append(std::move(indices));
  }

  void add_previous_instruction_indices(const MFInstruction *instruction,
                                        NextInstructionInfo &instr_info)
  {
    this->add_owned_indices(instruction, std::move(instr_info.owned_indices));
  }

  NextInstructionInfo pop_next()
  {
    if (indices_by_instruction_.is_empty()) {
      return {};
    }
    /* TODO: Implement better mechanism to determine next instruction. */
    const MFInstruction *instruction = *indices_by_instruction_.keys().begin();

    Vector<int64_t> indices = this->pop_indices_array(instruction);
    if (indices.is_empty()) {
      return {};
    }

    NextInstructionInfo next_instruction_info;
    next_instruction_info.instruction = instruction;
    next_instruction_info.owned_indices = std::move(indices);
    return next_instruction_info;
  }

 private:
  Vector<int64_t> pop_indices_array(const MFInstruction *instruction)
  {
    Vector<Vector<int64_t>> *indices = indices_by_instruction_.lookup_ptr(instruction);
    if (indices == nullptr) {
      return {};
    }
    Vector<int64_t> r_indices;
    while (!indices->is_empty() && r_indices.is_empty()) {
      r_indices = (*indices).pop_last();
    }
    if (indices->is_empty()) {
      indices_by_instruction_.remove_contained(instruction);
    }
    return r_indices;
  }
};

void MFProcedureExecutor::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (procedure_.entry() == nullptr) {
    return;
  }

  LinearAllocator<> allocator;

  VariableStoreContainer variable_stores{*this, procedure_, mask, params};

  InstructionScheduler scheduler;
  scheduler.add_referenced_indices(procedure_.entry(), mask);

  while (NextInstructionInfo instr_info = scheduler.pop_next()) {
    const MFInstruction &instruction = *instr_info.instruction;
    switch (instruction.type()) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instruction = static_cast<const MFCallInstruction &>(
            instruction);
        execute_call_instruction(call_instruction, instr_info.mask(), variable_stores, context);
        scheduler.add_previous_instruction_indices(call_instruction.next(), instr_info);
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instruction = static_cast<const MFBranchInstruction &>(
            instruction);
        const MFVariable *condition_var = branch_instruction.condition();
        VariableStore &store = variable_stores.get_store_for_variable(*condition_var);

        IndicesSplitVectors new_indices;
        store.indices_split(instr_info.mask(), new_indices);
        scheduler.add_owned_indices(branch_instruction.branch_false(), new_indices[false]);
        scheduler.add_owned_indices(branch_instruction.branch_true(), new_indices[true]);
        break;
      }
      case MFInstructionType::Destruct: {
        const MFDestructInstruction &destruct_instruction =
            static_cast<const MFDestructInstruction &>(instruction);
        const MFVariable *variable = destruct_instruction.variable();
        variable_stores.destruct(*variable, instr_info.mask());
        scheduler.add_previous_instruction_indices(destruct_instruction.next(), instr_info);
        break;
      }
    }
  }
}

}  // namespace blender::fn
