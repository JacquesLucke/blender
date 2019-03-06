#include "numeric_lists.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "backends/llvm/ir_utils.hpp"

namespace FN { namespace Types {

	class FloatListType : public CPPTypeInfo {
		uint size_of_type() const override
		{
			return sizeof(FloatList *);
		}

		void construct_default(void *ptr) const override
		{
			*(FloatList **)ptr = new FloatList();
		}

		void destruct_type(void *ptr) const override
		{
			(*(FloatList **)ptr)->remove_user();
		}

		void copy_to_initialized(void *src, void *dst) const override
		{
			this->destruct_type(dst);
			this->copy_to_uninitialized(src, dst);
		}

		void copy_to_uninitialized(void *src, void *dst) const override
		{
			FloatList *list = (FloatList *)src;
			list->new_user();
			(*(FloatList **)dst) = list;
		}
	};

	static void *copy_func(void *value)
	{
		FloatList *list = (FloatList *)value;
		list->new_user();
		return list;
	}

	static void free_func(void *value)
	{
		FloatList *list = (FloatList *)value;
		list->remove_user();
	}

	static void *default_func()
	{
		return new FloatList();
	}


	LAZY_INIT_REF__NO_ARG(SharedType, get_float_list_type)
	{
		SharedType type = SharedType::New("Float List");
		type->extend(new FloatListType());
		type->extend(new PointerLLVMTypeInfo(
			copy_func, free_func, default_func));
		return type;
	}

} } /* namespace FN::Types */