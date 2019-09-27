#include "tuple_call.hpp"

namespace FN {

void TupleCallBodyBase::init_defaults(Tuple &fn_in) const
{
  fn_in.init_default_all();
}

void TupleCallBodyBase::owner_init_post()
{
  m_meta_in = TupleMeta(this->owner().input_types());
  m_meta_out = TupleMeta(this->owner().output_types());
}

uint LazyInTupleCallBody::user_data_size() const
{
  return 0;
}

const Vector<uint> &LazyInTupleCallBody::always_required() const
{
  static Vector<uint> empty_list = {};
  return empty_list;
}

} /* namespace FN */
