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

#include "FN_multi_function_procedure.hh"

#include "NOD_derived_node_tree.hh"

namespace blender::nodes {

using namespace fn::multi_function_procedure_types;
using namespace fn::multi_function_types;

class NodeTreeProcedureBuilder;

class NodeMFProcedureBuilder {
 private:
  const DNode node_;
  NodeTreeProcedureBuilder &procedure_builder_;
  const MFInstruction *input_instruction_ = nullptr;
  const MFInstruction *output_instruction_ = nullptr;

  friend NodeTreeProcedureBuilder;

 public:
  NodeMFProcedureBuilder(const DNode node, NodeTreeProcedureBuilder &procedure_builder)
      : node_(node), procedure_builder_(procedure_builder)
  {
  }

  MFProcedure &procedure();

  bNode &bnode()
  {
    return *node_->bnode();
  }

  MFVariable &get_input(StringRef identifer);
  void set_output(StringRef identifier, MFVariable &variable);
  void set_input_instruction(MFInstruction &instruction);
  void set_output_instruction(MFInstruction &instruction);
  void set_matching_fn(const MultiFunction &fn);
};

struct MFProcedureFromNodes {
  std::unique_ptr<MFProcedure> procedure;
  Vector<DOutputSocket> inputs;
};

MFProcedureFromNodes create_multi_function_procedure(const DerivedNodeTree &tree,
                                                     Span<DSocket> tree_outputs);

}  // namespace blender::nodes
