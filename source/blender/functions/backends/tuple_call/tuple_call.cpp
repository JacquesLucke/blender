#include "tuple_call.hpp"

namespace FN {

	BLI_COMPOSITION_IMPLEMENTATION(TupleCallBody);

	void TupleCallBody::init_defaults(Tuple &fn_in) const
	{
		fn_in.init_default_all();
	}

	void TupleCallBody::owner_init_post()
	{
		m_meta_in = SharedTupleMeta::New(this->owner()->signature().input_types());
		m_meta_out = SharedTupleMeta::New(this->owner()->signature().output_types());
	}

} /* namespace FN */