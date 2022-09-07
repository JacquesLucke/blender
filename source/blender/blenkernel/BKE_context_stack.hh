/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_context_stack.hh"

namespace blender::bke {

class ModifierContextStack : public ContextStack {
 private:
  static constexpr const char *s_static_type = "MODIFIER";

  std::string modifier_name_;

 public:
  ModifierContextStack(const ContextStack *parent, std::string modifier_name);

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

class NodeGroupContextStack : public ContextStack {
 private:
  static constexpr const char *s_static_type = "NODE_GROUP";

  std::string node_name_;
  std::string debug_group_name_;

 public:
  NodeGroupContextStack(const ContextStack *parent,
                        std::string node_name,
                        std::string debug_group_name = "<unknown>");

  StringRefNull node_name() const;

 private:
  void print_current_in_line(std::ostream &stream) const override;
};

}  // namespace blender::bke
