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

#include "FN_multi_function_procedure.hh"

#include "BLI_dot_export.hh"

namespace blender::fn {

void MFVariable::set_name(std::string name)
{
  name_ = std::move(name);
}

void MFCallInstruction::set_next(MFInstruction *instruction)
{
  if (next_ != nullptr) {
    next_->prev_.remove_first_occurrence_and_reorder(this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(this);
  }
  next_ = instruction;
}

void MFCallInstruction::set_param_variable(int param_index, MFVariable *variable)
{
  if (params_[param_index] != nullptr) {
    params_[param_index]->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    BLI_assert(fn_->param_type(param_index).data_type() == variable->data_type());
    variable->users_.append(this);
  }
  params_[param_index] = variable;
}

void MFCallInstruction::set_params(Span<MFVariable *> variables)
{
  BLI_assert(variables.size() == params_.size());
  for (const int i : variables.index_range()) {
    this->set_param_variable(i, variables[i]);
  }
}

void MFBranchInstruction::set_condition(MFVariable *variable)
{
  if (condition_ != nullptr) {
    condition_->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    variable->users_.append(this);
  }
  condition_ = variable;
}

void MFBranchInstruction::set_branch_true(MFInstruction *instruction)
{
  if (branch_true_ != nullptr) {
    branch_true_->prev_.remove_first_occurrence_and_reorder(this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(this);
  }
  branch_true_ = instruction;
}

void MFBranchInstruction::set_branch_false(MFInstruction *instruction)
{
  if (branch_false_ != nullptr) {
    branch_false_->prev_.remove_first_occurrence_and_reorder(this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(this);
  }
  branch_false_ = instruction;
}

void MFDestructInstruction::set_variable(MFVariable *variable)
{
  if (variable_ != nullptr) {
    variable_->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    variable->users_.append(this);
  }
  variable_ = variable;
}

void MFDestructInstruction::set_next(MFInstruction *instruction)
{
  if (next_ != nullptr) {
    next_->prev_.remove_first_occurrence_and_reorder(this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(this);
  }
  next_ = instruction;
}

MFVariable &MFProcedure::new_variable(MFDataType data_type, std::string name)
{
  MFVariable &variable = *allocator_.construct<MFVariable>().release();
  variable.name_ = std::move(name);
  variable.data_type_ = data_type;
  variable.id_ = variables_.size();
  variables_.append(&variable);
  return variable;
}

MFCallInstruction &MFProcedure::new_call_instruction(const MultiFunction &fn)
{
  MFCallInstruction &instruction = *allocator_.construct<MFCallInstruction>().release();
  instruction.type_ = MFInstructionType::Call;
  instruction.fn_ = &fn;
  instruction.params_ = allocator_.allocate_array<MFVariable *>(fn.param_amount());
  instruction.params_.fill(nullptr);
  call_instructions_.append(&instruction);
  return instruction;
}

MFCallInstruction &MFProcedure::new_call_instruction(const MultiFunction &fn,
                                                     Span<MFVariable *> param_variables)
{
  MFCallInstruction &instruction = this->new_call_instruction(fn);
  instruction.set_params(param_variables);
  return instruction;
}

MFBranchInstruction &MFProcedure::new_branch_instruction(MFVariable *condition_variable)
{
  MFBranchInstruction &instruction = *allocator_.construct<MFBranchInstruction>().release();
  instruction.type_ = MFInstructionType::Branch;
  branch_instructions_.append(&instruction);
  instruction.set_condition(condition_variable);
  return instruction;
}

MFDestructInstruction &MFProcedure::new_destruct_instruction(MFVariable *variable)
{
  MFDestructInstruction &instruction = *allocator_.construct<MFDestructInstruction>().release();
  instruction.type_ = MFInstructionType::Destruct;
  destruct_instructions_.append(&instruction);
  instruction.set_variable(variable);
  return instruction;
}

DestructInstructionChain MFProcedure::new_destruct_instructions(Span<MFVariable *> variables)
{
  DestructInstructionChain chain;
  for (MFVariable *variable : variables) {
    MFDestructInstruction &instruction = this->new_destruct_instruction(variable);
    if (chain.first == nullptr) {
      chain.first = chain.last = &instruction;
    }
    else {
      chain.last->set_next(&instruction);
      chain.last = &instruction;
    }
  }
  return chain;
}

void MFProcedure::add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable)
{
  params_.append({interface_type, &variable});
}

void MFProcedure::set_entry(MFInstruction &entry)
{
  entry_ = &entry;
}

void MFProcedure::assert_valid() const
{
  /**
   * - Non parameter variables are destructed.
   * - At every instruction, every variable is either initialized or uninitialized.
   * - Input and mutable parameters of call instructions are initialized.
   * - Condition of branch instruction is initialized.
   * - Output parameters of call instructions are not initialized.
   * - Input parameters are never destructed.
   * - Mutable and output parameteres are initialized on every exit.
   * - No aliasing issues in call instructions (can happen when variable is used more than once).
   */
}

MFProcedure::~MFProcedure()
{
  for (MFCallInstruction *instruction : call_instructions_) {
    instruction->~MFCallInstruction();
  }
  for (MFBranchInstruction *instruction : branch_instructions_) {
    instruction->~MFBranchInstruction();
  }
  for (MFDestructInstruction *instruction : destruct_instructions_) {
    instruction->~MFDestructInstruction();
  }
  for (MFVariable *variable : variables_) {
    variable->~MFVariable();
  }
}

static std::string optional_variable_to_string(const MFVariable *variable)
{
  if (variable == nullptr) {
    return "<null>";
  }
  std::stringstream ss;
  ss << variable->name() << "$" << variable->id();
  return ss.str();
}

std::string MFProcedure::to_dot() const
{
  dot::DirectedGraph digraph;
  Map<MFInstruction *, dot::Node *> dot_nodes;

  for (MFCallInstruction *instruction : call_instructions_) {
    std::stringstream ss;
    const MultiFunction &fn = instruction->fn();
    ss << fn.name();
    ss << "(";
    for (const int param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      switch (param_type.interface_type()) {
        case MFParamType::Input: {
          ss << "in: ";
          break;
        }
        case MFParamType::Output: {
          ss << "out: ";
          break;
        }
        case MFParamType::Mutable: {
          ss << "mut: ";
          break;
        }
      }
      MFVariable *variable = instruction->params()[param_index];
      ss << optional_variable_to_string(variable);
      if (param_index < fn.param_amount() - 1) {
        ss << ", ";
      }
    }
    ss << ")";
    dot::Node &dot_node = digraph.new_node(ss.str());
    dot_node.set_shape(dot::Attr_shape::Rectangle);
    dot_nodes.add_new(instruction, &dot_node);
  }
  for (MFBranchInstruction *instruction : branch_instructions_) {
    MFVariable *variable = instruction->condition();
    std::stringstream ss;
    ss << "Branch: " << optional_variable_to_string(variable);
    dot::Node &dot_node = digraph.new_node(ss.str());
    dot_node.set_shape(dot::Attr_shape::Rectangle);
    dot_nodes.add_new(instruction, &dot_node);
  }
  for (MFDestructInstruction *instruction : destruct_instructions_) {
    MFVariable *variable = instruction->variable();
    std::stringstream ss;
    ss << "Destruct: " << optional_variable_to_string(variable);
    dot::Node &dot_node = digraph.new_node(ss.str());
    dot_node.set_shape(dot::Attr_shape::Rectangle);
    dot_nodes.add_new(instruction, &dot_node);
  }

  auto create_end_node = [&]() -> dot::Node & {
    dot::Node &node = digraph.new_node("");
    node.set_shape(dot::Attr_shape::Circle);
    return node;
  };

  auto add_edge_to_instruction_or_end = [&](dot::Node &dot_from, MFInstruction *to) {
    if (to == nullptr) {
      dot::Node &dot_end_node = create_end_node();
      digraph.new_edge(dot_from, dot_end_node);
    }
    else {
      dot::Node &dot_to = *dot_nodes.lookup(to);
      digraph.new_edge(dot_from, dot_to);
    }
  };

  for (MFCallInstruction *instruction : call_instructions_) {
    dot::Node &dot_node = *dot_nodes.lookup(instruction);
    add_edge_to_instruction_or_end(dot_node, instruction->next());
  }

  for (MFBranchInstruction *instruction : branch_instructions_) {
    dot::Node &dot_node = *dot_nodes.lookup(instruction);
    add_edge_to_instruction_or_end(dot_node, instruction->branch_true());
    add_edge_to_instruction_or_end(dot_node, instruction->branch_false());
  }

  for (MFDestructInstruction *instruction : destruct_instructions_) {
    dot::Node &dot_node = *dot_nodes.lookup(instruction);
    add_edge_to_instruction_or_end(dot_node, instruction->next());
  }

  dot::Node &dot_entry = digraph.new_node("Entry");
  add_edge_to_instruction_or_end(dot_entry, entry_);

  return digraph.to_dot_string();
}

}  // namespace blender::fn
