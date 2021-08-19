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

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_procedure.hh"

namespace blender::fn {

class MFInstructionCursor {
 private:
  MFInstruction *instruction_ = nullptr;
  /* Only used when it is a branch instruction. */
  bool branch_output_ = false;
  /* Only used when instruction is null. */
  bool is_entry_ = false;

 public:
  MFInstructionCursor() = default;

  MFInstructionCursor(MFCallInstruction &instruction) : instruction_(&instruction)
  {
  }

  MFInstructionCursor(MFDestructInstruction &instruction) : instruction_(&instruction)
  {
  }

  MFInstructionCursor(MFBranchInstruction &instruction, bool branch_output)
      : instruction_(&instruction), branch_output_(branch_output)
  {
  }

  static MFInstructionCursor Entry()
  {
    MFInstructionCursor cursor;
    cursor.is_entry_ = true;
    return cursor;
  }

  void insert(MFProcedure &procedure, MFInstruction *new_instruction)
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
      }
    }
  }
};

struct MFProcedureBuilderBranch;

class MFProcedureBuilder {
 private:
  MFProcedure *procedure_ = nullptr;
  Vector<MFInstructionCursor> cursors_;

 public:
  MFProcedureBuilder(MFProcedure &procedure,
                     MFInstructionCursor initial_cursor = MFInstructionCursor::Entry())
      : procedure_(&procedure), cursors_({initial_cursor})
  {
  }

  MFProcedureBuilder(Span<MFProcedureBuilder *> builders)
      : MFProcedureBuilder(*builders[0]->procedure_)
  {
    this->set_cursor(builders);
  }

  MFProcedureBuilder(MFProcedureBuilderBranch &branch);

  void set_cursor(const MFInstructionCursor &cursor)
  {
    cursors_ = {cursor};
  }

  void set_cursor(Span<MFInstructionCursor> cursors)
  {
    cursors_ = cursors;
  }

  void set_cursor_after_branch(MFProcedureBuilderBranch &branch);

  void set_cursor(Span<MFProcedureBuilder *> builders)
  {
    cursors_.clear();
    for (MFProcedureBuilder *builder : builders) {
      cursors_.extend(builder->cursors_);
    }
  }

  void insert_destruct(MFVariable &variable)
  {
    MFDestructInstruction &instruction = procedure_->new_destruct_instruction();
    instruction.set_variable(&variable);
    this->insert_at_cursors(&instruction);
    cursors_ = {MFInstructionCursor{instruction}};
  }

  void insert_destruct(Span<MFVariable *> variables)
  {
    for (MFVariable *variable : variables) {
      this->insert_destruct(*variable);
    }
  }

  MFProcedureBuilderBranch insert_branch(MFVariable &condition);

  MFCallInstruction &insert_call(const MultiFunction &fn)
  {
    MFCallInstruction &instruction = procedure_->new_call_instruction(fn);
    this->insert_at_cursors(&instruction);
    cursors_ = {MFInstructionCursor{instruction}};
    return instruction;
  }

  MFCallInstruction &insert_call(const MultiFunction &fn, Span<MFVariable *> variables)
  {
    MFCallInstruction &instruction = this->insert_call(fn);
    instruction.set_params(variables);
    return instruction;
  }

  Vector<MFVariable *> insert_call_with_new_variables(
      const MultiFunction &fn, Span<MFVariable *> input_and_mutable_variables = {})
  {
    Vector<MFVariable *> output_variables;
    MFCallInstruction &instruction = this->insert_call(fn);
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

  template<int OutputN>
  std::array<MFVariable *, OutputN> insert_call_with_new_variables(
      const MultiFunction &fn, Span<MFVariable *> input_and_mutable_variables = {})
  {
    Vector<MFVariable *> output_variables = this->insert_call_with_new_variables(
        fn, input_and_mutable_variables);
    BLI_assert(output_variables.size() == OutputN);

    std::array<MFVariable *, OutputN> output_array;
    initialized_copy_n(output_variables.data(), OutputN, output_array.data());
    return output_array;
  }

  void add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable)
  {
    procedure_->add_parameter(interface_type, variable);
  }

  MFVariable &add_parameter(MFParamType param_type, std::string name = "")
  {
    MFVariable &variable = procedure_->new_variable(param_type.data_type(), std::move(name));
    this->add_parameter(param_type.interface_type(), variable);
    return variable;
  }

  MFVariable &add_input_parameter(MFDataType data_type, std::string name = "")
  {
    return this->add_parameter(MFParamType(MFParamType::Input, data_type), std::move(name));
  }

  template<typename T> MFVariable &add_single_input_parameter(std::string name = "")
  {
    return this->add_parameter(MFParamType::ForSingleInput(CPPType::get<T>()), std::move(name));
  }

  template<typename T> MFVariable &add_single_mutable_parameter(std::string name = "")
  {
    return this->add_parameter(MFParamType::ForMutableSingle(CPPType::get<T>()), std::move(name));
  }

  void add_output_parameter(MFVariable &variable)
  {
    this->add_parameter(MFParamType::Output, variable);
  }

 private:
  void insert_at_cursors(MFInstruction *instruction)
  {
    for (MFInstructionCursor &cursor : cursors_) {
      cursor.insert(*procedure_, instruction);
    }
  }
};

struct MFProcedureBuilderBranch {
  MFProcedureBuilder branch_true;
  MFProcedureBuilder branch_false;
};

MFProcedureBuilder::MFProcedureBuilder(MFProcedureBuilderBranch &branch)
    : MFProcedureBuilder(*branch.branch_true.procedure_)
{
  this->set_cursor_after_branch(branch);
}

void MFProcedureBuilder::set_cursor_after_branch(MFProcedureBuilderBranch &branch)
{
  this->set_cursor({&branch.branch_false, &branch.branch_true});
}

MFProcedureBuilderBranch MFProcedureBuilder::insert_branch(MFVariable &condition)
{
  MFBranchInstruction &instruction = procedure_->new_branch_instruction();
  instruction.set_condition(&condition);
  this->insert_at_cursors(&instruction);

  MFProcedureBuilderBranch branch{*procedure_, *procedure_};
  branch.branch_true.set_cursor(MFInstructionCursor{instruction, true});
  branch.branch_false.set_cursor(MFInstructionCursor{instruction, false});
  return branch;
}

}  // namespace blender::fn
