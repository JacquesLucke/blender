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

#include "BLI_dot_export.hh"
#include "BLI_stack.hh"

namespace blender::fn::script {

static void add_reachable_instructions(const MFInstruction &entry,
                                       Set<const MFInstruction *> &r_instructions)
{
  Stack<const MFInstruction *> instr_to_check;
  instr_to_check.push(&entry);

  while (!instr_to_check.is_empty()) {
    const MFInstruction *instr = instr_to_check.pop();
    if (instr == nullptr) {
      continue;
    }
    if (!r_instructions.add(instr)) {
      continue;
    }
    switch (instr->type) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instr = *static_cast<const MFCallInstruction *>(instr);
        instr_to_check.push(call_instr.next);
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instr = *static_cast<const MFBranchInstruction *>(instr);
        instr_to_check.push(branch_instr.true_instruction);
        instr_to_check.push(branch_instr.false_instruction);
        break;
      }
    }
  }
}

static std::string call_instruction_to_string(const MFCallInstruction &instr)
{
  std::stringstream ss;
  ss << instr.function->name() << " - ";
  for (const int param_index : instr.function->param_indices()) {
    const MFParamType param_type = instr.function->param_type(param_index);
    ss << instr.registers[param_index]->name << "(";
    switch (param_type.interface_type()) {
      case MFParamType::InterfaceType::Input: {
        ss << "in";
        break;
      }
      case MFParamType::InterfaceType::Mutable: {
        ss << "mut";
        break;
      }
      case MFParamType::InterfaceType::Output: {
        ss << "out";
        break;
      }
    }
    ss << ")";
    if (param_index < instr.function->param_amount() - 1) {
      ss << ", ";
    }
  }
  return ss.str();
}

std::string MFScript::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::TopToBottom);

  Set<const MFInstruction *> instructions;
  add_reachable_instructions(*this->entry, instructions);

  Map<const MFInstruction *, dot::Node *> dot_nodes;
  for (const MFInstruction *instr : instructions) {
    switch (instr->type) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instr = *static_cast<const MFCallInstruction *>(instr);
        std::string node_name = call_instr.function->name();
        dot::Node &dot_node = digraph.new_node(call_instruction_to_string(call_instr));
        dot_node.set_shape(dot::Attr_shape::Rectangle);
        dot_nodes.add_new(instr, &dot_node);
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instr = *static_cast<const MFBranchInstruction *>(instr);
        dot::Node &dot_node = digraph.new_node("if " + branch_instr.condition->name);
        dot_node.attributes.set("ordering", "out");
        dot_node.set_shape(dot::Attr_shape::Rectangle);
        dot_nodes.add_new(instr, &dot_node);
        break;
      }
    }
  }

  auto create_return_node = [&]() {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_shape(dot::Attr_shape::Circle);
    dot_node.set_background_color("#DDDDDD");
    return &dot_node;
  };

  for (const MFInstruction *instr : instructions) {
    dot::Node *current_node = dot_nodes.lookup(instr);
    switch (instr->type) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instr = *static_cast<const MFCallInstruction *>(instr);
        dot::Node *next_node;
        if (call_instr.next == nullptr) {
          next_node = create_return_node();
        }
        else {
          next_node = dot_nodes.lookup(call_instr.next);
        }
        digraph.new_edge(*current_node, *next_node);
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instr = *static_cast<const MFBranchInstruction *>(instr);
        dot::Node *true_node;
        if (branch_instr.true_instruction == nullptr) {
          true_node = create_return_node();
        }
        else {
          true_node = dot_nodes.lookup(branch_instr.true_instruction);
        }
        dot::Node *false_node;
        if (branch_instr.true_instruction == nullptr) {
          false_node = create_return_node();
        }
        else {
          false_node = dot_nodes.lookup(branch_instr.false_instruction);
        }
        dot::DirectedEdge &true_edge = digraph.new_edge(*current_node, *true_node);
        true_edge.set_color("#33AA33");
        dot::DirectedEdge &false_edge = digraph.new_edge(*current_node, *false_node);
        false_edge.set_color("#AA3333");
        break;
      }
    }
  }

  dot::Node &entry_node = digraph.new_node("");
  dot::Node &entry_instr_node = *dot_nodes.lookup(this->entry);
  entry_node.set_background_color("#DDDDDD");
  entry_node.set_shape(dot::Attr_shape::Circle);
  digraph.new_edge(entry_node, entry_instr_node);

  return digraph.to_dot_string();
}

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
    const MFInstruction *instr = *masks_by_instruction.keys().begin();
    Vector<Vector<int64_t>> masks = masks_by_instruction.pop(instr);
    switch (instr->type) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instr = *static_cast<const MFCallInstruction *>(instr);
        for (Span<int64_t> sub_mask : masks) {
          const MultiFunction &fn = *call_instr.function;
          MFParamsBuilder params{fn, array_size};
          for (const int param_index : fn.param_indices()) {
            MFParamType param_type = fn.param_type(param_index);
            BLI_assert(param_type.data_type().is_single());
            const MFRegister *reg = call_instr.registers[param_index];
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
              case MFParamType::Category::SingleMutable: {
                params.add_single_mutable(span);
                break;
              }
              default: {
                BLI_assert(false);
              }
            }
          }

          fn.call(sub_mask, params, context);
        }
        if (call_instr.next != nullptr) {
          for (Vector<int64_t> &sub_mask : masks) {
            masks_by_instruction.lookup_or_add_default(call_instr.next)
                .append(std::move(sub_mask));
          }
        }
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instr = *static_cast<const MFBranchInstruction *>(instr);
        Span<bool> condition = register_buffers.lookup(branch_instr.condition).typed<bool>();
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
          if (branch_instr.true_instruction != nullptr && !true_mask.is_empty()) {
            masks_by_instruction.lookup_or_add_default(branch_instr.true_instruction)
                .append(std::move(true_mask));
          }
          if (branch_instr.false_instruction != nullptr && !false_mask.is_empty()) {
            masks_by_instruction.lookup_or_add_default(branch_instr.false_instruction)
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
