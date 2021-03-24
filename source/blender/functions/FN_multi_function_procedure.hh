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

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;

  MFVariable() = default;

 public:
  MFDataType data_type() const;
  Span<MFInstruction *> users();
};

class MFInstruction : NonCopyable, NonMovable {
 protected:
  MFInstructionType type_;
  Vector<MFInstruction *> prev_;

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;

  MFInstruction() = default;

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
  void set_next(MFInstruction *instruction);

  void set_param_variable(int param_index, MFVariable *variable);
  Span<MFVariable *> params();
};

class MFBranchInstruction : public MFInstruction {
 private:
  MFVariable *condition_ = nullptr;
  MFInstruction *branch_true_ = nullptr;
  MFInstruction *branch_false_ = nullptr;

 public:
  MFVariable *condition();
  void set_condition(MFVariable *variable);

  MFInstruction *branch_true();
  void set_branch_true(MFInstruction *instruction);

  MFInstruction *branch_false();
  void set_branch_false(MFInstruction *instruction);
};

class MFDestructInstruction : public MFInstruction {
 private:
  MFVariable *variable_ = nullptr;
  MFInstruction *next_ = nullptr;

 public:
  MFVariable *variable();
  void set_variable(MFVariable *variable);

  MFInstruction *next();
  void set_next(MFInstruction *instruction);
};

class MFProcedure : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<MFInstruction *> instructions_;
  Vector<MFVariable *> variables_;
  Vector<MFVariable *> inputs_;
  Vector<MFVariable *> outputs_;
  MFInstruction *entry = nullptr;

 public:
  MFProcedure() = default;
  ~MFProcedure();

  MFCallInstruction &new_call_instruction(const MultiFunction &fn);
  MFBranchInstruction &new_branch_instruction();
  MFDestructInstruction &new_destruct_instruction();

  void set_entry(const MFInstruction &entry);
};

class MFProcedureExecutor : public MultiFunction {
 private:
  MFSignature signature_;
  const MFProcedure &procedure_;

 public:
  MFProcedureExecutor(const MFProcedure &procedure);
};

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

inline Span<MFVariable *> MFCallInstruction::params()
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

inline MFInstruction *MFBranchInstruction::branch_true()
{
  return branch_true_;
}

inline MFInstruction *MFBranchInstruction::branch_false()
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

inline MFInstruction *MFDestructInstruction::next()
{
  return next_;
}

}  // namespace blender::fn
