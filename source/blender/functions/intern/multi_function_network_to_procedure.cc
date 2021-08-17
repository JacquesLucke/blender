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

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network_to_procedure.hh"

#include "BLI_set.hh"

namespace blender::fn {

namespace {
struct ConversionContext {
  MFProcedure &procedure;
  ResourceScope &scope;
  Map<const MFSocket *, MFVariable *> socket_variables;
  Vector<MFInstruction *> ordered_instructions;
};
}  // namespace

class CopyMultiFunction : public MultiFunction {
 private:
  MFDataType data_type_;
  MFSignature signature_;

 public:
  CopyMultiFunction(MFDataType data_type) : data_type_(data_type)
  {
    MFSignatureBuilder signature("Copy " + data_type.to_string());
    signature.input("In", data_type);
    signature.output("Out", data_type);
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    switch (data_type_.category()) {
      case MFDataType::Single: {
        const GVArray &values_in = params.readonly_single_input(0);
        GMutableSpan values_out = params.uninitialized_single_output(1);
        const CPPType &type = data_type_.single_type();
        BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
        for (int i : mask) {
          values_in.get_to_uninitialized(i, buffer);
          type.copy_construct(buffer, values_out[i]);
          type.destruct(buffer);
        }
        break;
      }
      case MFDataType::Vector: {
        const GVVectorArray &values_in = params.readonly_vector_input(0);
        GVectorArray &values_out = params.vector_output(1);
        values_out.extend(mask, values_in);
        break;
      }
    }
  }
};

static void add_instructions_to_compute_socket(const MFSocket &socket, ConversionContext &context)
{
  if (context.socket_variables.contains(&socket)) {
    return;
  }
  if (socket.is_input()) {
    const MFInputSocket &input_socket_to_compute = socket.as_input();
    const MFOutputSocket &origin_socket = *input_socket_to_compute.origin();
    add_instructions_to_compute_socket(origin_socket, context);
    MFVariable &variable = *context.socket_variables.lookup(&origin_socket);
    MFVariable &copied_variable = context.procedure.new_variable(variable.data_type(),
                                                                 socket.name());
    const MultiFunction &copy_fn = context.scope.construct<CopyMultiFunction>(
        "copy function", variable.data_type());
    MFCallInstruction &copy_instruction = context.procedure.new_call_instruction(
        copy_fn, {&variable, &copied_variable});
    context.ordered_instructions.append(&copy_instruction);
    context.socket_variables.add_new(&socket, &copied_variable);
  }
  else {
    const MFOutputSocket &output_socket_to_compute = socket.as_output();
    const MFFunctionNode &node = output_socket_to_compute.node().as_function();
    const MultiFunction &fn = node.function();
    Vector<MFVariable *> variables;
    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      switch (param_type.interface_type()) {
        case MFParamType::Input: {
          const MFInputSocket &input_socket = node.input_for_param(param_index);
          add_instructions_to_compute_socket(input_socket, context);
          MFVariable *input_variable = context.socket_variables.lookup(&input_socket);
          variables.append(input_variable);
          break;
        }
        case MFParamType::Mutable: {
          const MFInputSocket &input_socket = node.input_for_param(param_index);
          const MFOutputSocket &output_socket = node.output_for_param(param_index);
          add_instructions_to_compute_socket(input_socket, context);
          MFVariable *input_variable = context.socket_variables.lookup(&input_socket);
          MFVariable &mutable_variable = context.procedure.new_variable(output_socket.data_type(),
                                                                        output_socket.name());
          const MultiFunction &copy_fn = context.scope.construct<CopyMultiFunction>(
              "copy function", input_variable->data_type());
          MFCallInstruction &copy_instruction = context.procedure.new_call_instruction(
              copy_fn, {input_variable, &mutable_variable});
          context.ordered_instructions.append(&copy_instruction);
          context.socket_variables.add_new(&output_socket, &mutable_variable);
          variables.append(&mutable_variable);
          break;
        }
        case MFParamType::Output: {
          const MFOutputSocket &output_socket = node.output_for_param(param_index);
          MFVariable &output_variable = context.procedure.new_variable(output_socket.data_type(),
                                                                       output_socket.name());
          context.socket_variables.add_new(&output_socket, &output_variable);
          variables.append(&output_variable);
          break;
        }
      }
    }
    MFCallInstruction &call_fn_instruction = context.procedure.new_call_instruction(fn, variables);
    context.ordered_instructions.append(&call_fn_instruction);
  }
}

MFProcedure &network_to_procedure(Span<const MFSocket *> inputs,
                                  Span<const MFSocket *> outputs,
                                  ResourceScope &scope)
{
  MFProcedure &procedure = scope.construct<MFProcedure>(__func__);
  ConversionContext context = {procedure, scope};

  Set<MFVariable *> param_variables;
  for (const MFSocket *socket : inputs) {
    MFVariable &variable = procedure.new_variable(socket->data_type(), socket->name());
    context.socket_variables.add_new(socket, &variable);
    procedure.add_parameter(MFParamType::Input, variable);
    param_variables.add_new(&variable);
  }
  for (const MFSocket *socket : outputs) {
    add_instructions_to_compute_socket(*socket, context);
    MFVariable *variable = context.socket_variables.lookup(socket);
    param_variables.add_new(variable);
    procedure.add_parameter(MFParamType::Output, *variable);
  }
  for (MFVariable *variable : procedure.variables()) {
    if (!param_variables.contains(variable)) {
      MFDestructInstruction &destruct_instr = procedure.new_destruct_instruction(variable);
      context.ordered_instructions.append(&destruct_instr);
    }
  }

  for (const int i : IndexRange(context.ordered_instructions.size() - 1)) {
    MFInstruction *instr = context.ordered_instructions[i];
    MFInstruction *next_instr = context.ordered_instructions[i + 1];
    if (instr->type() == MFInstructionType::Call) {
      static_cast<MFCallInstruction *>(instr)->set_next(next_instr);
    }
    else if (instr->type() == MFInstructionType::Destruct) {
      static_cast<MFDestructInstruction *>(instr)->set_next(next_instr);
    }
  }

  procedure.set_entry(*context.ordered_instructions[0]);

  return procedure;
}

}  // namespace blender::fn
