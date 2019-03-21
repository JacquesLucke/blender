#include "../registry.hpp"

#include "FN_types.hpp"
#include "FN_functions.hpp"

#include "RNA_access.h"

namespace FN { namespace DataFlowNodes {

	static void load_float(PointerRNA *ptr, Tuple &tuple, uint index)
	{
		float value = RNA_float_get(ptr, "value");
		tuple.set<float>(index, value);
	}

	static void load_vector(PointerRNA *ptr, Tuple &tuple, uint index)
	{
		float vector[3];
		RNA_float_get_array(ptr, "value", vector);
		tuple.set<Types::Vector>(index, Types::Vector(vector));
	}

	static void load_integer(PointerRNA *ptr, Tuple &tuple, uint index)
	{
		int value = RNA_int_get(ptr, "value");
		tuple.set<int32_t>(index, value);
	}

	template<typename T>
	static void load_empty_list(PointerRNA *UNUSED(ptr), Tuple &tuple, uint index)
	{
		auto list = Types::SharedList<T>::New();
		tuple.move_in(index, list);
	}

	void initialize_socket_inserters(GraphInserters &inserters)
	{
		inserters.reg_socket_loader("Float", load_float);
		inserters.reg_socket_loader("Vector", load_vector);
		inserters.reg_socket_loader("Integer", load_integer);
		inserters.reg_socket_loader("Float List", load_empty_list<float>);
		inserters.reg_socket_loader("Vector List", load_empty_list<Types::Vector>);
		inserters.reg_socket_loader("Integer List", load_empty_list<int32_t>);
	}

} }