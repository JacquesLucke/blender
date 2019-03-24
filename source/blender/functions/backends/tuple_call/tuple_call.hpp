#pragma once

#include "tuple.hpp"

namespace FN {

	class TupleCallBody : public FunctionBody {
	private:
		SharedTupleMeta m_meta_in;
		SharedTupleMeta m_meta_out;

	protected:
		void owner_init_post() override;

		TupleCallBody()
			: FunctionBody() {}

	public:
		BLI_COMPOSITION_DECLARATION(TupleCallBody);

		virtual ~TupleCallBody() {};

		virtual void call(Tuple &fn_in, Tuple &fn_out) const = 0;
		virtual void init_defaults(Tuple &fn_in) const;

		SharedTupleMeta &meta_in()
		{
			return m_meta_in;
		}

		SharedTupleMeta &meta_out()
		{
			return m_meta_out;
		}
	};

} /* namespace FN */