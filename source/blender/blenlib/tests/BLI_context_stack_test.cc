/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_context_stack.hh"

#include "testing/testing.h"

namespace blender::tests {

class NamedContext : public ContextStack {
 private:
  static constexpr char *static_type = "NAMED";
  std::string name_;

 public:
  NamedContext(const ContextStack *parent, std::string name)
      : ContextStack(static_type, parent), name_(std::move(name))
  {
    hash_.mix_in(static_type, name_);
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Named: " << name_;
  }
};

TEST(context_stack, Basic)
{
  NamedContext a{nullptr, "First"};
  NamedContext b{&a, "Second"};
  NamedContext c{&b, "Third"};
  std::cout << c << "\n";
}

}  // namespace blender::tests
