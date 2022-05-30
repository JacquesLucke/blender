/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_context_stack.hh"
#include "BLI_context_stack_map.hh"

#include "testing/testing.h"

namespace blender::tests {

class NamedContext : public ContextStack {
 private:
  static constexpr const char *s_static_type = "NAMED";
  std::string name_;

 public:
  NamedContext(const ContextStack *parent, std::string name)
      : ContextStack(s_static_type, parent), name_(std::move(name))
  {
    hash_.mix_in(s_static_type, strlen(s_static_type));
    hash_.mix_in(name_.data(), name_.size());
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Named: " << name_;
  }
};

class IndexContext : public ContextStack {
 private:
  static constexpr const char *s_static_type = "INDEX";
  int64_t index_;

 public:
  IndexContext(const ContextStack *parent, const int64_t index)
      : ContextStack(s_static_type, parent), index_(index)
  {
    hash_.mix_in(s_static_type, strlen(s_static_type));
    hash_.mix_in(&index_, sizeof(index_));
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Index: " << index_;
  }
};

TEST(context_stack, Basic)
{
  ContextStackMap<int> map;

  {
    const NamedContext a{nullptr, "First"};
    const NamedContext b{&a, "Second"};
    const NamedContext c{&b, "Third"};
    const IndexContext d1{&c, 42};
    const IndexContext d2{&a, 100};

    map.lookup_or_add(b) = 10;
    map.lookup_or_add(d1) = 123;
  }
  {
    const NamedContext a{nullptr, "First"};
    const NamedContext b{&a, "Second"};
    const NamedContext c{&b, "Third"};
    const IndexContext d1{&c, 42};
    const IndexContext d2{&a, 100};
    const std::array<const ContextStack *, 5> elements = {&a, &b, &c, &d1, &d2};
    for (const ContextStack *v : elements) {
      v->print_stack(std::cout, std::to_string(map.lookup_or_default(*v, -1)));
    }
  }
}

}  // namespace blender::tests
