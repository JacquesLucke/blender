#include "switch.hpp"

#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	class BoolSwitch : public TupleCallBody {
	private:
		SharedType m_data_type;
		CPPTypeInfo *m_type_info;

	public:
		BoolSwitch(SharedType data_type)
			: m_data_type(data_type),
			  m_type_info(data_type->extension<CPPTypeInfo>()) {}

		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			bool condition = fn_in.get<bool>(0);
			uint size = m_type_info->size_of_type();
			void *value = alloca(size);
			if (condition) {
				fn_in.relocate_out__dynamic(1, value);
				fn_out.relocate_in__dynamic(0, value);
			}
			else {
				fn_in.relocate_out__dynamic(2, value);
				fn_out.relocate_in__dynamic(0, value);
			}
		}
	};

	static SharedFunction build_bool_switch_function(SharedType &data_type)
	{
		std::string name = "Switch " + data_type->name();
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("Condition", get_bool_type()),
			InputParameter("True", data_type),
			InputParameter("False", data_type),
		}, {
			OutputParameter("Result", data_type),
		}));
		fn->add_body(new BoolSwitch(data_type));
		return fn;
	}

	SharedFunction &bool_switch(SharedType &data_type)
	{
		static FunctionPerType functions;
		if (!functions.contains(data_type)) {
			SharedFunction fn = build_bool_switch_function(data_type);
			functions.add(data_type, fn);
		}
		return functions.lookup_ref(data_type);
	}

} } /* namespace FN::Functions */