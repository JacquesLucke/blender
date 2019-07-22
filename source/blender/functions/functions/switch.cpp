#include "switch.hpp"

#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

class LazyBoolSwitch : public LazyInTupleCallBody {
 private:
  SharedType m_type;
  uint m_type_size;
  const SmallVector<uint> m_always_required = {0};

 public:
  LazyBoolSwitch(SharedType type)
      : m_type(type), m_type_size(type->extension<CPPTypeInfo>()->size_of_type())
  {
  }

  const SmallVector<uint> &always_required() const override
  {
    return m_always_required;
  }

  void call(Tuple &fn_in,
            Tuple &fn_out,
            ExecutionContext &UNUSED(ctx),
            LazyState &state) const override
  {
    bool condition = fn_in.get<bool>(0);

    if (state.is_first_entry()) {
      if (condition) {
        state.request_input(1);
      }
      else {
        state.request_input(2);
      }
      return;
    }

    void *value = alloca(m_type_size);
    if (condition) {
      fn_in.relocate_out__dynamic(1, value);
    }
    else {
      fn_in.relocate_out__dynamic(2, value);
    }
    fn_out.relocate_in__dynamic(0, value);
    state.done();
  }
};

static SharedFunction build_bool_switch_function(SharedType &data_type)
{
  FunctionBuilder builder;
  builder.add_input("Condition", GET_TYPE_bool());
  builder.add_input("True", data_type);
  builder.add_input("False", data_type);
  builder.add_output("Result", data_type);

  std::string name = "Switch " + data_type->name();
  auto fn = builder.build(name);
  fn->add_body<LazyBoolSwitch>(data_type);
  return fn;
}

SharedFunction &GET_FN_bool_switch(SharedType &data_type)
{
  static Map<SharedType, SharedFunction> functions;
  if (!functions.contains(data_type)) {
    SharedFunction fn = build_bool_switch_function(data_type);
    functions.add(data_type, fn);
  }
  return functions.lookup_ref(data_type);
}

}  // namespace Functions
}  // namespace FN
