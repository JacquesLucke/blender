#include "tuple_call.hpp"
#include "lazy_to_normal.hpp"

namespace FN {

/**
 * This just turns a lazy tuple-call body into a normal one by calling it multiple times, until it
 * is done.
 */
class MakeEagerBody : public TupleCallBody {
 private:
  LazyInTupleCallBody &m_lazy_body;
  uint m_user_data_size;

 public:
  MakeEagerBody(LazyInTupleCallBody &body)
      : m_lazy_body(body), m_user_data_size(body.user_data_size())
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    void *user_data = alloca(m_user_data_size);
    LazyState state(user_data);
    while (!state.is_done()) {
      state.start_next_entry();
      m_lazy_body.call(fn_in, fn_out, ctx, state);
    }
  }
};

void derive_TupleCallBody_from_LazyInTupleCallBody(SharedFunction &fn)
{
  BLI_assert(fn->has_body<LazyInTupleCallBody>());
  BLI_assert(!fn->has_body<TupleCallBody>());

  fn->add_body<MakeEagerBody>(fn->body<LazyInTupleCallBody>());
}

} /* namespace FN */
