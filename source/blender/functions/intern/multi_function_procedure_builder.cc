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

#include "FN_multi_function_procedure_builder.hh"

namespace blender::fn {

void MFInstructionCursor::insert(MFProcedure &procedure, MFInstruction *new_instruction)
{
  if (instruction_ == nullptr) {
    if (is_entry_) {
      procedure.set_entry(*new_instruction);
    }
    else {
      /* The cursors points at nothing, nothing to do. */
    }
  }
  else {
    switch (instruction_->type()) {
      case MFInstructionType::Call: {
        static_cast<MFCallInstruction *>(instruction_)->set_next(new_instruction);
        break;
      }
      case MFInstructionType::Branch: {
        MFBranchInstruction &branch_instruction = *static_cast<MFBranchInstruction *>(
            instruction_);
        if (branch_output_) {
          branch_instruction.set_branch_true(new_instruction);
        }
        else {
          branch_instruction.set_branch_false(new_instruction);
        }
        break;
      }
      case MFInstructionType::Destruct: {
        static_cast<MFDestructInstruction *>(instruction_)->set_next(new_instruction);
        break;
      }
      case MFInstructionType::Dummy: {
        static_cast<MFDummyInstruction *>(instruction_)->set_next(new_instruction);
        break;
      }
    }
  }
}

void MFProcedureBuilder::add_destruct(MFVariable &variable)
{
  MFDestructInstruction &instruction = procedure_->new_destruct_instruction();
  instruction.set_variable(&variable);
  this->insert_at_cursors(&instruction);
  cursors_ = {MFInstructionCursor{instruction}};
}

void MFProcedureBuilder::add_destruct(Span<MFVariable *> variables)
{
  for (MFVariable *variable : variables) {
    this->add_destruct(*variable);
  }
}

MFCallInstruction &MFProcedureBuilder::add_call(const MultiFunction &fn)
{
  MFCallInstruction &instruction = procedure_->new_call_instruction(fn);
  this->insert_at_cursors(&instruction);
  cursors_ = {MFInstructionCursor{instruction}};
  return instruction;
}

MFCallInstruction &MFProcedureBuilder::add_call(const MultiFunction &fn,
                                                Span<MFVariable *> variables)
{
  MFCallInstruction &instruction = this->add_call(fn);
  instruction.set_params(variables);
  return instruction;
}

Vector<MFVariable *> MFProcedureBuilder::add_call_with_new_variables(
    const MultiFunction &fn, Span<MFVariable *> input_and_mutable_variables)
{
  Vector<MFVariable *> output_variables;
  MFCallInstruction &instruction = this->add_call(fn);
  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    switch (param_type.interface_type()) {
      case MFParamType::Input:
      case MFParamType::Mutable: {
        MFVariable *variable = input_and_mutable_variables.first();
        instruction.set_param_variable(param_index, variable);
        input_and_mutable_variables = input_and_mutable_variables.drop_front(1);
        break;
      }
      case MFParamType::Output: {
        MFVariable &variable = procedure_->new_variable(param_type.data_type());
        instruction.set_param_variable(param_index, &variable);
        output_variables.append(&variable);
        break;
      }
    }
  }
  /* All passed in variables should have been dropped in the loop above. */
  BLI_assert(input_and_mutable_variables.is_empty());
  return output_variables;
}

MFProcedureBuilder::Branch MFProcedureBuilder::add_branch(MFVariable &condition)
{
  MFBranchInstruction &instruction = procedure_->new_branch_instruction();
  instruction.set_condition(&condition);
  this->insert_at_cursors(&instruction);

  Branch branch{*procedure_, *procedure_};
  branch.branch_true.set_cursor(MFInstructionCursor{instruction, true});
  branch.branch_false.set_cursor(MFInstructionCursor{instruction, false});
  return branch;
}

}  // namespace blender::fn
