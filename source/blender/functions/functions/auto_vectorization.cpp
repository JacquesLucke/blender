#include <cmath>

#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

namespace FN { namespace Functions {

	class AutoVectorization : public TupleCallBody {
	private:
		SharedFunction m_main;
		TupleCallBody *m_main_body;

		const SmallVector<bool> m_input_is_list;
		SmallVector<uint> m_list_inputs;

		SmallVector<TupleCallBody *> m_get_length_bodies;
		uint m_max_len_in_size, m_max_len_out_size;

		SmallVector<TupleCallBody *> m_get_element_bodies;
		SmallVector<TupleCallBody *> m_create_empty_bodies;
		SmallVector<TupleCallBody *> m_append_bodies;

	public:
		AutoVectorization(
			SharedFunction main,
			const SmallVector<bool> &input_is_list)
			: m_main(main),
			  m_main_body(main->body<TupleCallBody>()),
			  m_input_is_list(input_is_list)
			{
				for (uint i = 0; i < input_is_list.size(); i++) {
					if (input_is_list[i]) {
						m_list_inputs.append(i);
					}
				}
				for (uint i : m_list_inputs) {
					SharedType &base_type = main->signature().inputs()[i].type();
					m_get_length_bodies.append(list_length(base_type)->body<TupleCallBody>());
					m_get_element_bodies.append(get_list_element(base_type)->body<TupleCallBody>());
				}

				m_max_len_in_size = 0;
				m_max_len_out_size = 0;
				for (TupleCallBody *body : m_get_length_bodies) {
					m_max_len_in_size = std::max(m_max_len_in_size, body->meta_in()->size_of_full_tuple());
					m_max_len_out_size = std::max(m_max_len_out_size, body->meta_out()->size_of_full_tuple());
				}

				for (auto output : main->signature().outputs()) {
					SharedType &base_type = output.type();
					m_create_empty_bodies.append(empty_list(base_type)->body<TupleCallBody>());
					m_append_bodies.append(append_to_list(base_type)->body<TupleCallBody>());
				}
			}

		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
		{
			uint *input_lengths = BLI_array_alloca(input_lengths, m_list_inputs.size());
			this->get_input_list_lengths(fn_in, ctx, input_lengths);

			uint max_length = 0;
			for (uint i = 0; i < m_list_inputs.size(); i++) {
				max_length = std::max(max_length, input_lengths[i]);
			}

			ctx.print_with_traceback("Final Length: " + std::to_string(max_length));

			this->initialize_empty_lists(fn_out, ctx);

			FN_TUPLE_STACK_ALLOC(main_in, m_main_body->meta_in());
			FN_TUPLE_STACK_ALLOC(main_out, m_main_body->meta_out());

			for (uint iteration = 0; iteration < max_length; iteration++) {
				uint list_index = 0;
				for (uint i = 0; i < m_input_is_list.size(); i++) {
					if (m_input_is_list[i]) {
						this->copy_in_iteration(iteration, fn_in, main_in, i, list_index, input_lengths[list_index], ctx);
						list_index++;
					}
					else {
						Tuple::copy_element(fn_in, i, main_in, i);
					}
				}

				m_main_body->call(main_in, main_out, ctx);

				for (uint i = 0; i < m_main->signature().outputs().size(); i++) {
					this->append_to_output(main_out, fn_out, i, ctx);
				}
			}
		}

	private:
		void get_input_list_lengths(Tuple &fn_in, ExecutionContext &ctx, uint *r_lengths) const
		{
			void *buf_in = alloca(m_max_len_in_size);
			void *buf_out = alloca(m_max_len_out_size);

			for (uint i = 0; i < m_list_inputs.size(); i++) {
				uint index = m_list_inputs[i];
				TupleCallBody *body = m_get_length_bodies[i];

				Tuple &len_in = Tuple::ConstructInBuffer(body->meta_in(), buf_in);
				Tuple &len_out = Tuple::ConstructInBuffer(body->meta_out(), buf_out);

				Tuple::copy_element(fn_in, index, len_in, 0);

				body->call__setup_stack(len_in, len_out, ctx);

				uint length = len_out.get<uint>(0);
				r_lengths[i] = length;

				len_in.~Tuple();
				len_out.~Tuple();
			}
		}

		void copy_in_iteration(uint iteration, Tuple &fn_in, Tuple &main_in, uint index, uint list_index, uint list_length, ExecutionContext &ctx) const
		{
			if (list_length == 0) {
				main_in.init_default(index);
				return;
			}

			TupleCallBody *body = m_get_element_bodies[list_index];

			uint load_index = iteration % list_length;
			FN_TUPLE_STACK_ALLOC(get_element_in, body->meta_in());
			FN_TUPLE_STACK_ALLOC(get_element_out, body->meta_out());

			Tuple::copy_element(fn_in, index, get_element_in, 0);
			get_element_in.set<uint>(1, load_index);
			get_element_in.init_default(2);

			body->call__setup_stack(get_element_in, get_element_out, ctx);

			Tuple::relocate_element(get_element_out, 0, main_in, index);
		}

		void initialize_empty_lists(Tuple &fn_out, ExecutionContext &ctx) const
		{
			for (uint i = 0; i < m_main->signature().outputs().size(); i++) {
				this->initialize_empty_list(fn_out, i, ctx);
			}
		}

		void initialize_empty_list(Tuple &fn_out, uint index, ExecutionContext &ctx) const
		{
			TupleCallBody *body = m_create_empty_bodies[index];

			FN_TUPLE_STACK_ALLOC(create_list_in, body->meta_in());
			FN_TUPLE_STACK_ALLOC(create_list_out, body->meta_out());

			body->call__setup_stack(create_list_in, create_list_out, ctx);

			Tuple::relocate_element(create_list_out, 0, fn_out, index);
		}

		void append_to_output(Tuple &main_out, Tuple &fn_out, uint index, ExecutionContext &ctx) const
		{
			TupleCallBody *body = m_append_bodies[index];

			FN_TUPLE_STACK_ALLOC(append_in, body->meta_in());
			FN_TUPLE_STACK_ALLOC(append_out, body->meta_out());

			Tuple::relocate_element(fn_out, index, append_in, 0);
			Tuple::relocate_element(main_out, index, append_in, 1);

			body->call__setup_stack(append_in, append_out, ctx);

			Tuple::relocate_element(append_out, index, fn_out, index);
		}
	};

	static bool any_true(const SmallVector<bool> &list)
	{
		for (bool value : list) {
			if (value) {
				return true;
			}
		}
		return false;
	}

	SharedFunction auto_vectorization(
		SharedFunction &original_fn,
		const SmallVector<bool> &vectorize_input)
	{
		uint input_amount = original_fn->signature().inputs().size();
		uint output_amount = original_fn->signature().outputs().size();

		BLI_assert(vectorize_input.size() == input_amount);
		BLI_assert(any_true(vectorize_input));

		InputParameters inputs;
		for (uint i = 0; i < input_amount; i++) {
			auto original_parameter = original_fn->signature().inputs()[i];
			if (vectorize_input[i]) {
				SharedType &list_type = get_list_type(original_parameter.type());
				inputs.append(InputParameter(original_parameter.name() + " (List)", list_type));
			}
			else {
				inputs.append(original_parameter);
			}
		}

		OutputParameters outputs;
		for (uint i = 0; i < output_amount; i++) {
			auto original_parameter = original_fn->signature().outputs()[i];
			SharedType &list_type = get_list_type(original_parameter.type());
			outputs.append(OutputParameter(original_parameter.name() + " (List)", list_type));
		}

		std::string name = original_fn->name() + " (Vectorized)";
		auto fn = SharedFunction::New(name, Signature(inputs, outputs));
		fn->add_body(new AutoVectorization(original_fn, vectorize_input));
		return fn;
	}

} } /* namespace FN::Functions */