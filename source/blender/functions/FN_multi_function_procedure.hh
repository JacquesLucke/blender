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

#include "FN_multi_function.hh"

namespace blender::fn {

class MFVariable;
class MFInstruction;
class MFCallInstruction;
class MFBranchInstruction;
class MFDestructInstruction;
class MFProcedure;

enum class MFInstructionType {
  Call,
  Branch,
  Destruct,
};

class MFVariable : NonCopyable, NonMovable {
 private:
  MFDataType data_type_;
  Vector<MFInstruction *> users_;
  std::string name_;
  int id_;

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;

 public:
  MFDataType data_type() const;
  Span<MFInstruction *> users();

  StringRefNull name() const;
  void set_name(std::string name);

  int id() const;
};

class MFInstruction : NonCopyable, NonMovable {
 protected:
  MFInstructionType type_;
  Vector<MFInstruction *> prev_;

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;

 public:
  MFInstructionType type() const;
  Span<MFInstruction *> prev();
};

class MFCallInstruction : public MFInstruction {
 private:
  const MultiFunction *fn_ = nullptr;
  MFInstruction *next_ = nullptr;
  MutableSpan<MFVariable *> params_;

  friend MFProcedure;

 public:
  const MultiFunction &fn() const;

  MFInstruction *next();
  const MFInstruction *next() const;
  void set_next(MFInstruction *instruction);

  void set_param_variable(int param_index, MFVariable *variable);
  void set_params(Span<MFVariable *> variables);
  Span<MFVariable *> params();
  Span<const MFVariable *> params() const;
};

class MFBranchInstruction : public MFInstruction {
 private:
  MFVariable *condition_ = nullptr;
  MFInstruction *branch_true_ = nullptr;
  MFInstruction *branch_false_ = nullptr;

 public:
  MFVariable *condition();
  const MFVariable *condition() const;
  void set_condition(MFVariable *variable);

  MFInstruction *branch_true();
  const MFInstruction *branch_true() const;
  void set_branch_true(MFInstruction *instruction);

  MFInstruction *branch_false();
  const MFInstruction *branch_false() const;
  void set_branch_false(MFInstruction *instruction);
};

class MFDestructInstruction : public MFInstruction {
 private:
  MFVariable *variable_ = nullptr;
  MFInstruction *next_ = nullptr;

 public:
  MFVariable *variable();
  const MFVariable *variable() const;
  void set_variable(MFVariable *variable);

  MFInstruction *next();
  const MFInstruction *next() const;
  void set_next(MFInstruction *instruction);
};

struct DestructInstructionChain {
  MFDestructInstruction *first = nullptr;
  MFDestructInstruction *last = nullptr;
};

class MFProcedure : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<MFCallInstruction *> call_instructions_;
  Vector<MFBranchInstruction *> branch_instructions_;
  Vector<MFDestructInstruction *> destruct_instructions_;
  Vector<MFVariable *> variables_;
  Vector<std::pair<MFParamType::InterfaceType, MFVariable *>> params_;
  MFInstruction *entry_ = nullptr;

 public:
  MFProcedure() = default;
  ~MFProcedure();

  MFVariable &new_variable(MFDataType data_type, std::string name = "");
  MFCallInstruction &new_call_instruction(const MultiFunction &fn);
  MFCallInstruction &new_call_instruction(const MultiFunction &fn,
                                          Span<MFVariable *> param_variables);
  MFBranchInstruction &new_branch_instruction(MFVariable *condition_variable = nullptr);
  MFDestructInstruction &new_destruct_instruction(MFVariable *variable = nullptr);
  DestructInstructionChain new_destruct_instructions(Span<MFVariable *> variables);

  void add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable);

  Span<std::pair<MFParamType::InterfaceType, const MFVariable *>> params() const;

  MFInstruction *entry();
  const MFInstruction *entry() const;
  void set_entry(MFInstruction &entry);

  Span<MFVariable *> variables();
  Span<const MFVariable *> variables() const;

  void assert_valid() const;

  std::string to_dot() const;
};

namespace multi_function_procedure_types {
using MFVariable = fn::MFVariable;
using MFInstruction = fn::MFInstruction;
using MFCallInstruction = fn::MFCallInstruction;
using MFBranchInstruction = fn::MFBranchInstruction;
using MFDestructInstruction = fn::MFDestructInstruction;
using MFProcedure = fn::MFProcedure;
}  // namespace multi_function_procedure_types

/* --------------------------------------------------------------------
 * MFVariable inline methods.
 */

inline MFDataType MFVariable::data_type() const
{
  return data_type_;
}

inline Span<MFInstruction *> MFVariable::users()
{
  return users_;
}

inline StringRefNull MFVariable::name() const
{
  return name_;
}

inline int MFVariable::id() const
{
  return id_;
}

/* --------------------------------------------------------------------
 * MFInstruction inline methods.
 */

inline MFInstructionType MFInstruction::type() const
{
  return type_;
}

inline Span<MFInstruction *> MFInstruction::prev()
{
  return prev_;
}

/* --------------------------------------------------------------------
 * MFCallInstruction inline methods.
 */

inline const MultiFunction &MFCallInstruction::fn() const
{
  return *fn_;
}

inline MFInstruction *MFCallInstruction::next()
{
  return next_;
}

inline const MFInstruction *MFCallInstruction::next() const
{
  return next_;
}

inline Span<MFVariable *> MFCallInstruction::params()
{
  return params_;
}

inline Span<const MFVariable *> MFCallInstruction::params() const
{
  return params_;
}

/* --------------------------------------------------------------------
 * MFBranchInstruction inline methods.
 */

inline MFVariable *MFBranchInstruction::condition()
{
  return condition_;
}

inline const MFVariable *MFBranchInstruction::condition() const
{
  return condition_;
}

inline MFInstruction *MFBranchInstruction::branch_true()
{
  return branch_true_;
}

inline const MFInstruction *MFBranchInstruction::branch_true() const
{
  return branch_true_;
}

inline MFInstruction *MFBranchInstruction::branch_false()
{
  return branch_false_;
}

inline const MFInstruction *MFBranchInstruction::branch_false() const
{
  return branch_false_;
}

/* --------------------------------------------------------------------
 * MFDestructInstruction inline methods.
 */

inline MFVariable *MFDestructInstruction::variable()
{
  return variable_;
}

inline const MFVariable *MFDestructInstruction::variable() const
{
  return variable_;
}

inline MFInstruction *MFDestructInstruction::next()
{
  return next_;
}

inline const MFInstruction *MFDestructInstruction::next() const
{
  return next_;
}

/* --------------------------------------------------------------------
 * MFProcedure inline methods.
 */

inline Span<std::pair<MFParamType::InterfaceType, const MFVariable *>> MFProcedure::params() const
{
  return params_.as_span().cast<std::pair<MFParamType::InterfaceType, const MFVariable *>>();
}

inline MFInstruction *MFProcedure::entry()
{
  return entry_;
}

inline const MFInstruction *MFProcedure::entry() const
{
  return entry_;
}

inline Span<MFVariable *> MFProcedure::variables()
{
  return variables_;
}

inline Span<const MFVariable *> MFProcedure::variables() const
{
  return variables_;
}

}  // namespace blender::fn
