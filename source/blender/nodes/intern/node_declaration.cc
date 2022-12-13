/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_node.h"

namespace blender::nodes {

void build_node_declaration(const bNodeType &typeinfo, NodeDeclaration &r_declaration)
{
  NodeDeclarationBuilder node_decl_builder{r_declaration};
  typeinfo.declare(node_decl_builder);
  node_decl_builder.finalize();
}

void NodeDeclarationBuilder::finalize()
{
  if (is_function_node_) {
    for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : input_builders_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      if (socket_decl.input_field_type_ != InputSocketFieldType::Implicit) {
        socket_decl.input_field_type_ = InputSocketFieldType::IsSupported;
      }
    }
    for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : output_builders_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      socket_decl.output_field_dependency_ = OutputFieldDependency::ForDependentField();
      socket_builder->reference_pass_all_ = true;
    }
  }
  for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : input_builders_) {
    if (socket_builder->reference_on_auto_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      for (const int input_i : declaration_.inputs_.index_range()) {
        SocketDeclaration &other_socket_decl = *declaration_.inputs_[input_i];
        if (dynamic_cast<decl::Geometry *>(&other_socket_decl)) {
          socket_decl.input_reference_info_.available_on.append(input_i);
        }
      }
    }
  }
  for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : output_builders_) {
    if (socket_builder->reference_on_auto_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      socket_decl.output_reference_info_.available_on.emplace();
      for (const int output_i : declaration_.outputs_.index_range()) {
        SocketDeclaration &other_socket_decl = *declaration_.outputs_[output_i];
        if (dynamic_cast<decl::Geometry *>(&other_socket_decl)) {
          socket_decl.output_reference_info_.available_on->append(output_i);
        }
      }
    }
    if (socket_builder->reference_pass_all_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      for (const int input_i : declaration_.inputs_.index_range()) {
        socket_decl.output_reference_info_.pass_from.append(input_i);
      }
    }
    if (socket_builder->propagate_from_auto_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      for (const int input_i : declaration_.inputs_.index_range()) {
        SocketDeclaration &other_socket_decl = *declaration_.inputs_[input_i];
        if (dynamic_cast<decl::Geometry *>(&other_socket_decl)) {
          socket_decl.output_reference_info_.propagate_from.append(input_i);
        }
      }
    }
  }
}

bool NodeDeclaration::matches(const bNode &node) const
{
  auto check_sockets = [&](ListBase sockets, Span<SocketDeclarationPtr> socket_decls) {
    const int tot_sockets = BLI_listbase_count(&sockets);
    if (tot_sockets != socket_decls.size()) {
      return false;
    }
    int i;
    LISTBASE_FOREACH_INDEX (const bNodeSocket *, socket, &sockets, i) {
      const SocketDeclaration &socket_decl = *socket_decls[i];
      if (!socket_decl.matches(*socket)) {
        return false;
      }
    }
    return true;
  };

  if (!check_sockets(node.inputs, inputs_)) {
    return false;
  }
  if (!check_sockets(node.outputs, outputs_)) {
    return false;
  }
  return true;
}

bNodeSocket &SocketDeclaration::update_or_build(bNodeTree &ntree,
                                                bNode &node,
                                                bNodeSocket &socket) const
{
  /* By default just rebuild. */
  BLI_assert(socket.in_out == in_out_);
  UNUSED_VARS_NDEBUG(socket);
  return this->build(ntree, node);
}

void SocketDeclaration::set_common_flags(bNodeSocket &socket) const
{
  SET_FLAG_FROM_TEST(socket.flag, compact_, SOCK_COMPACT);
  SET_FLAG_FROM_TEST(socket.flag, hide_value_, SOCK_HIDE_VALUE);
  SET_FLAG_FROM_TEST(socket.flag, hide_label_, SOCK_HIDE_LABEL);
  SET_FLAG_FROM_TEST(socket.flag, is_multi_input_, SOCK_MULTI_INPUT);
  SET_FLAG_FROM_TEST(socket.flag, no_mute_links_, SOCK_NO_INTERNAL_LINK);
  SET_FLAG_FROM_TEST(socket.flag, is_unavailable_, SOCK_UNAVAIL);
}

bool SocketDeclaration::matches_common_data(const bNodeSocket &socket) const
{
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  if (((socket.flag & SOCK_COMPACT) != 0) != compact_) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_VALUE) != 0) != hide_value_) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_LABEL) != 0) != hide_label_) {
    return false;
  }
  if (((socket.flag & SOCK_MULTI_INPUT) != 0) != is_multi_input_) {
    return false;
  }
  if (((socket.flag & SOCK_NO_INTERNAL_LINK) != 0) != no_mute_links_) {
    return false;
  }
  if (((socket.flag & SOCK_UNAVAIL) != 0) != is_unavailable_) {
    return false;
  }
  return true;
}

namespace implicit_field_inputs {

void position(const bNode & /*node*/, void *r_value)
{
  new (r_value) fn::ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>("position"));
}

void normal(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      fn::ValueOrField<float3>(fn::Field<float3>(std::make_shared<bke::NormalFieldInput>()));
}

void index(const bNode & /*node*/, void *r_value)
{
  new (r_value) fn::ValueOrField<int>(fn::Field<int>(std::make_shared<fn::IndexFieldInput>()));
}

void id_or_index(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      fn::ValueOrField<int>(fn::Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
}

}  // namespace implicit_field_inputs

}  // namespace blender::nodes
