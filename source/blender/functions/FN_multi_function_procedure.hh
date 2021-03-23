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

enum class MFInstructionType {
  Call,
  Branch,
};

class MFVariable : NonCopyable, NonMovable {
 private:
  MFDataType data_type;
  Vector<MFInstruction *> users_;
};

class MFInstruction : NonCopyable, NonMovable {
 protected:
  MFInstructionType type_;
};

class MFCallInstruction : public MFInstruction {
 private:
  const MultiFunction *fn_ = nullptr;
  MFInstruction *next_ = nullptr;
  MutableSpan<MFVariable *> params_;
};

class MFBranchInstruction : public MFInstruction {
 private:
  MFVariable *condition_ = nullptr;
  MFInstruction *branch_true_ = nullptr;
  MFInstruction *branch_false_ = nullptr;
};

class MFProcedure : NonCopyable, NonMovable {
 private:
  Vector<MFInstruction *> instructions_;
  Vector<MFVariable *> variables_;
  Vector<MFVariable *> inputs_;
  Vector<MFVariable *> outputs_;
  MFInstruction *entry = nullptr;
};

class MFProcedureExecutor : public MultiFunction {
 private:
  MFSignature signature_;
  const MFProcedure &procedure_;

 public:
  MFProcedureExecutor(const MFProcedure &procedure);
};
}  // namespace blender::fn
