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
  VirtualSingleFromCaller,
  SingleFromCaller,
  VirtualVectorFromCaller,
  VectorFromCaller,
  SingleOwn,
  VectorOwn,
};

struct VariableStore {
  VariableStoreType type;

  VariableStore(VariableStoreType type) : type(type)
  {
  }
};

struct VariableStore_VirtualSingleFromCaller : public VariableStore {
  const GVArray &data;

  VariableStore_VirtualSingleFromCaller(const GVArray &data)
      : VariableStore(VariableStoreType::VirtualSingleFromCaller), data(data)
  {
  }
};

struct VariableStore_SingleFromCaller : public VariableStore {
  GMutableSpan data;

  VariableStore_SingleFromCaller(GMutableSpan data)
      : VariableStore(VariableStoreType::SingleFromCaller), data(data)
  {
  }
};

struct VariableStore_VirtualVectorFromCaller : public VariableStore {
  const GVVectorArray &data;

  VariableStore_VirtualVectorFromCaller(const GVVectorArray &data)
      : VariableStore(VariableStoreType::VirtualVectorFromCaller), data(data)
  {
  }
};

struct VariableStore_VectorFromCaller : public VariableStore {
  GVectorArray &data;

  VariableStore_VectorFromCaller(GVectorArray &data)
      : VariableStore(VariableStoreType::VectorFromCaller), data(data)
  {
  }
};

struct VariableStore_SingleOwn : public VariableStore {
  GMutableSpan data;
  int64_t tot_initialized = 0;

  VariableStore_SingleOwn(GMutableSpan data)
      : VariableStore(VariableStoreType::SingleOwn), data(data)
  {
  }
};

struct VariableStore_VectorOwn : public VariableStore {
  GVectorArray &data;
  int64_t tot_initialized = 0;

  VariableStore_VectorOwn(GVectorArray &data)
      : VariableStore(VariableStoreType::VectorOwn), data(data)
  {
  }
};

class VariableStoreContainer {
 public:
  LinearAllocator<> allocator_;
  Map<const MFVariable *, VariableStore *> stores_;

  VariableStoreContainer() = default;

  void add_from_caller(const MFProcedureExecutor &fn,
                       const MFProcedure &procedure,
                       MFParams &params)
  {
    for (const int param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      const MFVariable *variable = procedure.params()[param_index].second;
      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &data = params.readonly_single_input(param_index);
          stores_.add_new(
              variable,
              allocator_.construct<VariableStore_VirtualSingleFromCaller>(data).release());
          break;
        }
        case MFParamType::VectorInput: {
          const GVVectorArray &data = params.readonly_vector_input(param_index);
          stores_.add_new(
              variable,
              allocator_.construct<VariableStore_VirtualVectorFromCaller>(data).release());
          break;
        }
        case MFParamType::SingleOutput: {
          GMutableSpan data = params.uninitialized_single_output(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_SingleFromCaller>(data).release());
          break;
        }
        case MFParamType::VectorOutput: {
          GVectorArray &data = params.vector_output(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_VectorFromCaller>(data).release());
          break;
        }
        case MFParamType::SingleMutable: {
          GMutableSpan data = params.single_mutable(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_SingleFromCaller>(data).release());
          break;
        }
        case MFParamType::VectorMutable: {
          GVectorArray &data = params.vector_mutable(param_index);
          stores_.add_new(variable,
                          allocator_.construct<VariableStore_VectorFromCaller>(data).release());
          break;
        }
      }
    }
  }
};

}  // namespace

static void add_input_from_store(MFParamsBuilder &params, VariableStore &store)
{
  switch (store.type) {
    case VariableStoreType::VirtualSingleFromCaller: {
      params.add_readonly_single_input(
          static_cast<VariableStore_VirtualSingleFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::SingleFromCaller: {
      params.add_readonly_single_input(static_cast<VariableStore_SingleFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::VirtualVectorFromCaller: {
      params.add_readonly_vector_input(
          static_cast<VariableStore_VirtualVectorFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::VectorFromCaller: {
      params.add_readonly_vector_input(static_cast<VariableStore_VectorFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::SingleOwn: {
      params.add_readonly_single_input(static_cast<VariableStore_SingleOwn &>(store).data);
      break;
    }
    case VariableStoreType::VectorOwn: {
      params.add_readonly_vector_input(static_cast<VariableStore_VectorOwn &>(store).data);
      break;
    }
  }
}

static void add_mutable_from_store(MFParamsBuilder &params, VariableStore &store)
{
  switch (store.type) {
    case VariableStoreType::SingleFromCaller: {
      params.add_single_mutable(static_cast<VariableStore_SingleFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::VectorFromCaller: {
      params.add_vector_mutable(static_cast<VariableStore_VectorFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::SingleOwn: {
      params.add_single_mutable(static_cast<VariableStore_SingleOwn &>(store).data);
      break;
    }
    case VariableStoreType::VectorOwn: {
      params.add_vector_mutable(static_cast<VariableStore_VectorOwn &>(store).data);
      break;
    }
    case VariableStoreType::VirtualSingleFromCaller:
    case VariableStoreType::VirtualVectorFromCaller: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void add_output_from_store(MFParamsBuilder &params, VariableStore &store)
{
  switch (store.type) {
    case VariableStoreType::SingleFromCaller: {
      params.add_uninitialized_single_output(
          static_cast<VariableStore_SingleFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::VectorFromCaller: {
      params.add_vector_output(static_cast<VariableStore_VectorFromCaller &>(store).data);
      break;
    }
    case VariableStoreType::SingleOwn: {
      params.add_uninitialized_single_output(static_cast<VariableStore_SingleOwn &>(store).data);
      break;
    }
    case VariableStoreType::VectorOwn: {
      params.add_vector_output(static_cast<VariableStore_VectorOwn &>(store).data);
      break;
    }
    case VariableStoreType::VirtualSingleFromCaller:
    case VariableStoreType::VirtualVectorFromCaller: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static int64_t update_tot_initialized_if_necessary(VariableStore &store, const int64_t change)
{
  switch (store.type) {
    case VariableStoreType::SingleOwn: {
      VariableStore_SingleOwn &own_store = static_cast<VariableStore_SingleOwn &>(store);
      own_store.tot_initialized += change;
      return own_store.tot_initialized;
    }
    case VariableStoreType::VectorOwn: {
      VariableStore_VectorOwn &own_store = static_cast<VariableStore_VectorOwn &>(store);
      own_store.tot_initialized += change;
      return own_store.tot_initialized;
    }
    case VariableStoreType::SingleFromCaller:
    case VariableStoreType::VectorFromCaller:
    case VariableStoreType::VirtualSingleFromCaller:
    case VariableStoreType::VirtualVectorFromCaller: {
      break;
    }
  }
  return -1;
}

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
        add_input_from_store(params, *store);
        break;
      }
      case MFParamType::Mutable: {
        add_mutable_from_store(params, *store);
        break;
      }
      case MFParamType::Output: {
        add_output_from_store(params, *store);
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

  LinearAllocator<> allocator;

  VariableStoreContainer variable_stores;
  variable_stores.add_from_caller(*this, procedure_, params);

  const int64_t min_array_size = mask.min_array_size();
  for (const MFVariable *variable : procedure_.variables()) {
    if (variable_stores.stores_.contains(variable)) {
      continue;
    }
    MFDataType data_type = variable->data_type();
    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single_type();
        void *buffer = allocator.allocate(type.size() * min_array_size, type.alignment());
        variable_stores.stores_.add_new(
            variable,
            allocator
                .construct<VariableStore_SingleOwn>(GMutableSpan(type, buffer, min_array_size))
                .release());
        break;
      }
      case MFDataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        /* TODO: Free. */
        GVectorArray *vector_array = new GVectorArray(type, min_array_size);
        variable_stores.stores_.add_new(
            variable, allocator.construct<VariableStore_VectorOwn>(*vector_array).release());
        break;
      }
    }
  }

  Map<const MFInstruction *, Vector<Vector<int64_t>>> indices_by_instruction;

  indices_by_instruction.add_new(procedure_.entry(), {mask.indices()});
  while (!indices_by_instruction.is_empty()) {
    const MFInstruction *instruction = *indices_by_instruction.keys().begin();
    Vector<Vector<int64_t>> indices_vector = indices_by_instruction.pop(instruction);
    switch (instruction->type()) {
      case MFInstructionType::Call: {
        const MFCallInstruction *call_instruction = static_cast<const MFCallInstruction *>(
            instruction);
        for (Span<int64_t> indices : indices_vector) {
          execute_call_instruction(*call_instruction, indices, variable_stores.stores_, context);
        }
        const MFInstruction *next_instruction = call_instruction->next();
        if (next_instruction != nullptr) {
          indices_by_instruction.lookup_or_add_default(next_instruction)
              .extend(std::move(indices_vector));
        }
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction *branch_instruction = static_cast<const MFBranchInstruction *>(
            instruction);
        const MFVariable *condition_var = branch_instruction->condition();
        VariableStore *store = variable_stores.stores_.lookup(condition_var);
        Vector<Vector<int64_t>> indices_vector_true, indices_vector_false;
        for (Span<int64_t> indices : indices_vector) {
          std::array<Vector<int64_t>, 2> new_indices;
          switch (store->type) {
            case VariableStoreType::SingleOwn: {
              Span<bool> conditions =
                  static_cast<VariableStore_SingleOwn *>(store)->data.typed<bool>();
              for (const int i : indices) {
                new_indices[conditions[i]].append(i);
              }
              break;
            }
            case VariableStoreType::SingleFromCaller: {
              Span<bool> conditions =
                  static_cast<VariableStore_SingleOwn *>(store)->data.typed<bool>();
              for (const int i : indices) {
                new_indices[conditions[i]].append(i);
              }
              break;
            }
            case VariableStoreType::VirtualSingleFromCaller: {
              const GVArray &conditions =
                  static_cast<VariableStore_VirtualSingleFromCaller *>(store)->data;
              for (const int i : indices) {
                bool condition;
                conditions.get(i, &condition);
                new_indices[condition].append(i);
              }
              break;
            }
            case VariableStoreType::VectorFromCaller:
            case VariableStoreType::VirtualVectorFromCaller:
            case VariableStoreType::VectorOwn: {
              BLI_assert_unreachable();
              break;
            }
          }
          indices_vector_false.append(std::move(new_indices[false]));
          indices_vector_true.append(std::move(new_indices[true]));
        }
        const MFInstruction *false_branch = branch_instruction->branch_false();
        if (false_branch != nullptr) {
          indices_by_instruction.lookup_or_add_default(false_branch)
              .extend(std::move(indices_vector_false));
        }
        const MFInstruction *true_branch = branch_instruction->branch_true();
        if (true_branch != nullptr) {
          indices_by_instruction.lookup_or_add_default(true_branch)
              .extend(std::move(indices_vector_true));
        }

        break;
      }
      case MFInstructionType::Destruct: {
        break;
      }
    }
  }
}

}  // namespace blender::fn
