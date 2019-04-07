#include "numeric_lists.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN { namespace Types {

	template<typename T>
	class ListLLVMTypeInfo : public LLVMTypeInfo {
	private:
		static void *copy_func(void *value)
		{
			List<T> *list = (List<T> *)value;
			list->new_user();
			return list;
		}

		static void free_func(void *value)
		{
			List<T> *list = (List<T> *)value;
			list->remove_user();
		}

		static void *default_func()
		{
			return new List<T>();
		}

	public:
		static LLVMTypeInfo *Create()
		{
			static_assert(sizeof(SharedList<T>) == sizeof(List<T> *),
				"Currently it is assumed that only a pointer to the list is stored");
			return new PointerLLVMTypeInfo(
				copy_func, free_func, default_func);
		}
	};

	template<typename T>
	SharedType create_list_type(std::string name)
	{
		SharedType type = SharedType::New(name);
		type->extend(new CPPTypeInfoForType<SharedList<T>>());
		type->extend(ListLLVMTypeInfo<T>::Create());
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_float_list)
	{
		return create_list_type<float>("Float List");
	}

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_fvec3_list)
	{
		return create_list_type<Vector>("FVec3 List");
	}

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_int32_list)
	{
		return create_list_type<int32_t>("Int32 List");
	}

} } /* namespace FN::Types */
