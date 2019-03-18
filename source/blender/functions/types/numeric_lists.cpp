#include "numeric_lists.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "backends/llvm/ir_utils.hpp"

namespace FN { namespace Types {

	template<typename T>
	class ListCPPTypeInfo : public CPPTypeInfo {
	private:
		static List<T> *list_from_ptr(void *ptr)
		{
			return ((SharedList<T> *)ptr)->ptr();
		}

	public:
		uint size_of_type() const override
		{
			return sizeof(SharedList<T>);
		}

		void construct_default(void *ptr) const override
		{
			List<T> *list = new List<T>();
			*(SharedList<T> *)ptr = SharedList<T>::FromPointer(list);
		}

		void destruct_type(void *ptr) const override
		{
			List<T> *list = this->list_from_ptr(ptr);
			list->remove_user();
		}

		void copy_to_initialized(void *src, void *dst) const override
		{
			this->destruct_type(dst);
			this->copy_to_uninitialized(src, dst);
		}

		void copy_to_uninitialized(void *src, void *dst) const override
		{
			List<T> *list = this->list_from_ptr(src);
			list->new_user();
			*(SharedList<T> *)dst = SharedList<T>::FromPointer(list);
		}
	};

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
		type->extend(new ListCPPTypeInfo<T>());
		type->extend(ListLLVMTypeInfo<T>::Create());
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_float_list_type)
	{
		return create_list_type<float>("Float List");
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_fvec3_list_type)
	{
		return create_list_type<Vector>("FVec3 List");
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_int32_list_type)
	{
		return create_list_type<int32_t>("Int32 List");
	}

} } /* namespace FN::Types */