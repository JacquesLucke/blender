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

#include "FN_multi_function_script.hh"

namespace blender::fn::script {

MFScriptEvaluator::MFScriptEvaluator(const MFScript &script) : script_(script)
{
  MFSignatureBuilder signature = this->get_builder("Script Evaluator");
  for (const MFRegister *reg : script_.input_registers) {
    BLI_assert(reg->data_type.is_single());
    signature.input("In", reg->data_type);
  }
  for (const MFRegister *reg : script_.output_registers) {
    BLI_assert(reg->data_type.is_single());
    signature.output("Out", reg->data_type);
  }
}

void MFScriptEvaluator::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (script_.entry == nullptr) {
    return;
  }

  const int array_size = mask.min_array_size();
  GuardedAllocator allocator;

  Map<const MFRegister *, GMutableSpan> register_buffers;
  Vector<GMutableSpan> buffers_to_destruct_and_free;

  for (const int i : script_.input_registers.index_range()) {
    const MFRegister *reg = script_.input_registers[i];
    GVSpan input_values = params.readonly_single_input(i);
    const CPPType &type = reg->data_type.single_type();
    void *buffer = allocator.allocate(array_size * type.size(), type.alignment(), __func__);
    input_values.materialize_to_uninitialized(mask, buffer);
    GMutableSpan span{type, buffer, array_size};
    register_buffers.add_new(reg, span);
    buffers_to_destruct_and_free.append(span);
  }

  for (const int i : script_.output_registers.index_range()) {
    const int param_index = script_.input_registers.size() + i;
    const MFRegister *reg = script_.output_registers[i];
    GMutableSpan output_values = params.uninitialized_single_output(param_index);
    const CPPType &type = reg->data_type.single_type();
    type.construct_default_indices(output_values.data(), mask);
    register_buffers.add_new(reg, output_values);
  }

  for (const MFRegister *reg : script_.registers) {
    if (register_buffers.contains(reg)) {
      continue;
    }
    const CPPType &type = reg->data_type.single_type();
    void *buffer = allocator.allocate(array_size * type.size(), type.alignment(), __func__);
    type.construct_default_indices(buffer, mask);
    GMutableSpan span{type, buffer, array_size};
    register_buffers.add_new(reg, span);
    buffers_to_destruct_and_free.append(span);
  }

  Map<const MFInstruction *, Vector<Vector<int64_t>>> masks_by_instruction;
  masks_by_instruction.add_new(script_.entry, {mask.indices()});

  while (!masks_by_instruction.is_empty()) {
    const MFInstruction *current_instr = *masks_by_instruction.keys().begin();
    Vector<Vector<int64_t>> masks = masks_by_instruction.pop(current_instr);
    switch (current_instr->type) {
      case MFInstructionType::Call: {
        const MFCallInstruction &instr = *static_cast<const MFCallInstruction *>(current_instr);
        for (Span<int64_t> sub_mask : masks) {
          const MultiFunction &fn = *instr.function;
          MFParamsBuilder params{fn, array_size};
          for (const int param_index : fn.param_indices()) {
            MFParamType param_type = fn.param_type(param_index);
            BLI_assert(param_type.data_type().is_single());
            const MFRegister *reg = instr.registers[param_index];
            GMutableSpan span = register_buffers.lookup(reg);
            switch (param_type.category()) {
              case MFParamType::Category::SingleInput: {
                params.add_readonly_single_input(span);
                break;
              }
              case MFParamType::Category::SingleOutput: {
                const CPPType &type = span.type();
                type.destruct_indices(span.data(), mask);
                params.add_uninitialized_single_output(span);
                break;
              }
              default: {
                BLI_assert(false);
              }
            }
          }

          fn.call(sub_mask, params, context);
        }
        if (instr.next != nullptr) {
          for (Vector<int64_t> &sub_mask : masks) {
            masks_by_instruction.lookup_or_add_default(instr.next).append(std::move(sub_mask));
          }
        }
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &instr = *static_cast<const MFBranchInstruction *>(
            current_instr);
        Span<bool> condition = register_buffers.lookup(instr.condition).typed<bool>();
        for (Span<int64_t> sub_mask : masks) {
          Vector<int64_t> true_mask, false_mask;
          for (int64_t i : sub_mask) {
            if (condition[i]) {
              true_mask.append(i);
            }
            else {
              false_mask.append(i);
            }
          }
          if (instr.true_instruction != nullptr) {
            masks_by_instruction.lookup_or_add_default(instr.true_instruction)
                .append(std::move(true_mask));
          }
          if (instr.false_instruction != nullptr) {
            masks_by_instruction.lookup_or_add_default(instr.false_instruction)
                .append(std::move(false_mask));
          }
        }
        break;
      }
    }
  }

  for (GMutableSpan span : buffers_to_destruct_and_free) {
    const CPPType &type = span.type();
    type.destruct_indices(span.data(), mask);
    allocator.deallocate(span.data());
  }
}

}  // namespace blender::fn::script
