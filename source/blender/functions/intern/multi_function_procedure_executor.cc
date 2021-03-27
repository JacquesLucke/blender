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

namespace {
enum class VariableStoreType {
  GVArray,
  GMutableSpan,
};

struct VariableStore {
  VariableStoreType type;

  VariableStore(VariableStoreType type) : type(type)
  {
  }
};

struct VariableStore_GVArray : public VariableStore {
  const GVArray &data;

  VariableStore_GVArray(const GVArray &data)
      : VariableStore(VariableStoreType::GVArray), data(data)
  {
  }
};

struct VariableStore_GMutableSpan : public VariableStore {
  GMutableSpan data;

  VariableStore_GMutableSpan(GMutableSpan data)
      : VariableStore(VariableStoreType::GMutableSpan), data(data)
  {
  }
};
}  // namespace

static void execute_call_instruction(const MFCallInstruction &instruction,
                                     IndexMask mask,
                                     Map<const MFVariable *, VariableStore *> &variable_stores,
                                     const MFContext &context)
{
  const MultiFunction &fn = instruction.fn();
  MFParamsBuilder params(fn, mask.min_array_size());

  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    const MFVariable *variable = instruction.params()[param_index];
    VariableStore *store = variable_stores.lookup(variable);

    switch (param_type.interface_type()) {
      case MFParamType::Input: {
        if (store->type == VariableStoreType::GVArray) {
          params.add_readonly_single_input(static_cast<VariableStore_GVArray *>(store)->data);
        }
        else if (store->type == VariableStoreType::GMutableSpan) {
          params.add_readonly_single_input(static_cast<VariableStore_GMutableSpan *>(store)->data);
        }
        else {
          BLI_assert_unreachable();
        }
        break;
      }
      case MFParamType::Mutable: {
        if (store->type == VariableStoreType::GMutableSpan) {
          params.add_single_mutable(static_cast<VariableStore_GMutableSpan *>(store)->data);
        }
        else {
          BLI_assert_unreachable();
        }
        break;
      }
      case MFParamType::Output: {
        if (store->type == VariableStoreType::GMutableSpan) {
          params.add_uninitialized_single_output(
              static_cast<VariableStore_GMutableSpan *>(store)->data);
        }
        else {
          BLI_assert_unreachable();
        }
        break;
      }
    }
  }

  fn.call(mask, params, context);
}

void MFProcedureExecutor::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (procedure_.entry() == nullptr) {
    return;
  }

  Map<const MFVariable *, VariableStore *> variable_stores;
  LinearAllocator<> allocator;

  for (const int param_index : procedure_.params().index_range()) {
    const std::pair<MFParamType::InterfaceType, const MFVariable *> &param =
        procedure_.params()[param_index];
    MFParamType::InterfaceType interface_type = param.first;
    const MFVariable *variable = param.second;
    /* For now. */
    BLI_assert(variable->data_type().is_single());
    switch (interface_type) {
      case MFParamType::Input: {
        const GVArray &data = params.readonly_single_input(param_index);
        variable_stores.add_new(variable,
                                allocator.construct<VariableStore_GVArray>(data).release());
        break;
      }
      case MFParamType::Mutable: {
        GMutableSpan data = params.single_mutable(param_index);
        variable_stores.add_new(variable,
                                allocator.construct<VariableStore_GMutableSpan>(data).release());
        break;
      }
      case MFParamType::Output: {
        GMutableSpan data = params.uninitialized_single_output(param_index);
        variable_stores.add_new(variable,
                                allocator.construct<VariableStore_GMutableSpan>(data).release());
        break;
      }
    }
  }
  const int64_t min_array_size = mask.min_array_size();
  for (const MFVariable *variable : procedure_.variables()) {
    if (variable_stores.contains(variable)) {
      continue;
    }
    /* For now. */
    BLI_assert(variable->data_type().is_single());

    const CPPType &type = variable->data_type().single_type();
    void *buffer = allocator.allocate(type.size() * min_array_size, type.alignment());
    variable_stores.add_new(
        variable,
        allocator.construct<VariableStore_GMutableSpan>(GMutableSpan(type, buffer, min_array_size))
            .release());
  }

  Map<const MFInstruction *, Vector<Array<int64_t>>> indices_by_instruction;

  indices_by_instruction.add_new(procedure_.entry(), {mask.indices()});
  while (!indices_by_instruction.is_empty()) {
    const MFInstruction *instruction = *indices_by_instruction.keys().begin();
    Vector<Array<int64_t>> indices_vector = indices_by_instruction.pop(instruction);
    switch (instruction->type()) {
      case MFInstructionType::Call: {
        const MFCallInstruction *call_instruction = static_cast<const MFCallInstruction *>(
            instruction);
        for (Span<int64_t> indices : indices_vector) {
          execute_call_instruction(*call_instruction, indices, variable_stores, context);
        }
        const MFInstruction *next_instruction = call_instruction->next();
        if (next_instruction != nullptr) {
          indices_by_instruction.lookup_or_add_default(next_instruction)
              .extend(std::move(indices_vector));
        }
        break;
      }
      case MFInstructionType::Branch: {
        break;
      }
      case MFInstructionType::Destruct: {
        break;
      }
    }
  }
}

}  // namespace blender::fn
