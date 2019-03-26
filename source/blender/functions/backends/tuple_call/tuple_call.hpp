#pragma once

#include "tuple.hpp"
#include "execution_context.hpp"

namespace FN {

	class TupleCallBodyBase : public FunctionBody {
	private:
		SharedTupleMeta m_meta_in;
		SharedTupleMeta m_meta_out;

	protected:
		void owner_init_post() override;

	public:
		virtual ~TupleCallBodyBase() {};

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

	class TupleCallBody : public TupleCallBodyBase {
	public:
		BLI_COMPOSITION_DECLARATION(TupleCallBody);

		virtual void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const = 0;
	};

	class LazyState {
	private:
		uint m_entry_count = 0;
		bool m_is_done = false;
		void *m_user_data;
		SmallVector<uint> m_requested_inputs;

	public:
		LazyState(void *user_data)
			: m_user_data(user_data) {}

		void start_next_entry()
		{
			m_entry_count++;
			m_requested_inputs.clear();
		}

		void request_input(uint index)
		{
			m_requested_inputs.append(index);
		}

		void done()
		{
			m_is_done = true;
		}

		const SmallVector<uint> &requested_inputs() const
		{
			return m_requested_inputs;
		}

		bool is_first_entry() const
		{
			return m_entry_count == 1;
		}

		bool is_done() const
		{
			return m_is_done;
		}

		void *user_data() const
		{
			return m_user_data;
		}
	};

	class LazyInTupleCallBody : public TupleCallBodyBase {
	public:
		BLI_COMPOSITION_DECLARATION(LazyInTupleCallBody);

		virtual uint user_data_size() const;
		virtual const SmallVector<uint> &always_required() const;
		virtual void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx, LazyState &state) const = 0;
	};

} /* namespace FN */