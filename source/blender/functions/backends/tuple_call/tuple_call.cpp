#include "tuple_call.hpp"

namespace FN {

BLI_COMPOSITION_IMPLEMENTATION(TupleCallBody);
BLI_COMPOSITION_IMPLEMENTATION(LazyInTupleCallBody);

void TupleCallBodyBase::init_defaults(Tuple &fn_in) const
{
  fn_in.init_default_all();
}

void TupleCallBodyBase::owner_init_post()
{
  m_meta_in = SharedTupleMeta::New(this->owner()->input_types());
  m_meta_out = SharedTupleMeta::New(this->owner()->output_types());
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
