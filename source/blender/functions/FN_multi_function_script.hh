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

#include "FN_multi_function.hh"

namespace blender::fn::script {

struct MFRegister {
  MFDataType data_type;
  std::string name;
};

enum class MFInstructionType {
  Call,
  Branch,
};

struct MFInstruction {
  MFInstructionType type;

  MFInstruction(MFInstructionType type) : type(type)
  {
  }
};

struct MFCallInstruction : public MFInstruction {
  const MultiFunction *function = nullptr;
  Vector<MFRegister *> registers;
  MFInstruction *next = nullptr;

  MFCallInstruction() : MFInstruction(MFInstructionType::Call)
  {
  }
};

struct MFBranchInstruction : public MFInstruction {
  MFRegister *condition = nullptr;
  MFInstruction *true_instruction = nullptr;
  MFInstruction *false_instruction = nullptr;

  MFBranchInstruction() : MFInstruction(MFInstructionType::Branch)
  {
  }
};

struct MFScript {
  MFInstruction *entry = nullptr;
  Vector<MFRegister *> registers;
  Vector<MFRegister *> input_registers;
  Vector<MFRegister *> output_registers;

  std::string to_dot() const;
};

class MFScriptEvaluator : public MultiFunction {
 private:
  const MFScript &script_;

 public:
  MFScriptEvaluator(const MFScript &script);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace blender::fn::script
