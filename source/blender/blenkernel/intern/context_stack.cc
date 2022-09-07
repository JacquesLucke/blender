/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context_stack.hh"

namespace blender::bke {

ModifierContextStack::ModifierContextStack(const ContextStack *parent, std::string modifier_name)
    : ContextStack(s_static_type, parent), modifier_name_(std::move(modifier_name))
{
  hash_.mix_in(s_static_type, strlen(s_static_type));
  hash_.mix_in(modifier_name_.data(), modifier_name_.size());
}

void ModifierContextStack::print_current_in_line(std::ostream &stream) const
{
  stream << "Modifier: " << modifier_name_;
}

NodeGroupContextStack::NodeGroupContextStack(const ContextStack *parent,
                                             std::string node_name,
                                             std::string debug_group_name)
    : ContextStack(s_static_type, parent),
      node_name_(std::move(node_name)),
      debug_group_name_(std::move(debug_group_name))
{
  hash_.mix_in(s_static_type, strlen(s_static_type));
  hash_.mix_in(node_name_.data(), node_name_.size());
}

StringRefNull NodeGroupContextStack::node_name() const
{
  return node_name_;
}

void NodeGroupContextStack::print_current_in_line(std::ostream &stream) const
{
  stream << "Node Group: " << debug_group_name_ << " \t Node Name: " << node_name_;
}

}  // namespace blender::bke
