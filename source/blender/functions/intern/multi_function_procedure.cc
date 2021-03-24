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
    BLI_assert(fn_->param_type(param_index) == variable->data_type());
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

}  // namespace blender::fn
