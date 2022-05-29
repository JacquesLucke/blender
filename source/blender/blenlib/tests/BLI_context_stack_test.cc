/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_context_stack.hh"

#include "testing/testing.h"

namespace blender::tests {

class NamedContext : public ContextStack {
 private:
  static constexpr const char *static_type = "NAMED";
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

class IndexContext : public ContextStack {
 private:
  static constexpr const char *static_type = "INDEX";
  int64_t index_;

 public:
  IndexContext(const ContextStack *parent, const int64_t index)
      : ContextStack(static_type, parent), index_(index)
  {
    hash_.mix_in(Span<std::byte>(reinterpret_cast<std::byte *>(&index_), sizeof(index_)));
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Index: " << index_;
  }
};

TEST(context_stack, Basic)
{
  NamedContext a{nullptr, "First"};
  NamedContext b{&a, "Second"};
  NamedContext c{&b, "Third"};
  IndexContext d1{&c, 42};
  IndexContext d2{&a, 100};
  std::cout << d1 << "\n";
  std::cout << d2 << "\n";
}

}  // namespace blender::tests
