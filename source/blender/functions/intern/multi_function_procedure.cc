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
  variables_.append(&variable);
  return variable;
}

MFCallInstruction &MFProcedure::new_call_instruction(const MultiFunction &fn)
{
  MFCallInstruction &instruction = *allocator_.construct<MFCallInstruction>().release();
  instruction.fn_ = &fn;
  instruction.params_ = allocator_.allocate_array<MFVariable *>(fn.param_amount());
  instruction.params_.fill(nullptr);
  instructions_.append(&instruction);
  return instruction;
}

MFBranchInstruction &MFProcedure::new_branch_instruction()
{
  MFBranchInstruction &instruction = *allocator_.construct<MFBranchInstruction>().release();
  instructions_.append(&instruction);
  return instruction;
}

MFDestructInstruction &MFProcedure::new_destruct_instruction()
{
  MFDestructInstruction &instruction = *allocator_.construct<MFDestructInstruction>().release();
  instructions_.append(&instruction);
  return instruction;
}

void MFProcedure::add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable)
{
  params_.append({interface_type, &variable});
}

void MFProcedure::set_entry(MFInstruction &entry)
{
  entry_ = &entry;
}

MFProcedure::~MFProcedure()
{
  for (MFInstruction *instruction : instructions_) {
    switch (instruction->type()) {
      case MFInstructionType::Call: {
        ((MFCallInstruction *)instruction)->~MFCallInstruction();
        break;
      }
      case MFInstructionType::Branch: {
        ((MFBranchInstruction *)instruction)->~MFBranchInstruction();
        break;
      }
      case MFInstructionType::Destruct: {
        ((MFDestructInstruction *)instruction)->~MFDestructInstruction();
        break;
      }
    }
  }
  for (MFVariable *variable : variables_) {
    variable->~MFVariable();
  }
}

}  // namespace blender::fn
