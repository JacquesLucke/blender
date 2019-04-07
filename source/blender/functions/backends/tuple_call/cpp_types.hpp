#pragma once

#include "FN_core.hpp"

namespace FN {

	class CPPTypeInfo : public TypeExtension {
	public:
		BLI_COMPOSITION_DECLARATION(CPPTypeInfo);

		virtual ~CPPTypeInfo() {}

		virtual uint size_of_type() const = 0;
		virtual void construct_default(void *ptr) const = 0;
		virtual void destruct_type(void *ptr) const = 0;
		virtual void copy_to_initialized(void *src, void *dst) const = 0;
		virtual void copy_to_uninitialized(void *src, void *dst) const = 0;
		virtual void relocate_to_initialized(void *src, void *dst) const = 0;
		virtual void relocate_to_uninitialized(void *src, void *dst) const = 0;
	};

	template<typename T>
	class CPPTypeInfoForType : public CPPTypeInfo {
	public:
		virtual uint size_of_type() const override
		{
			return sizeof(T);
		}

		virtual void construct_default(void *ptr) const override
		{
			new(ptr) T();
		}

		virtual void destruct_type(void *ptr) const override
		{
			T *ptr_ = (T *)ptr;
			ptr_->~T();
		}

		virtual void copy_to_initialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			std::copy(src_, src_ + 1, dst_);
		}

		virtual void copy_to_uninitialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			std::uninitialized_copy(src_, src_ + 1, dst_);
		}

		virtual void relocate_to_initialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			*dst_ = std::move(*src_);
			src_->~T();
		}

		virtual void relocate_to_uninitialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			std::uninitialized_copy(
				std::make_move_iterator(src_),
				std::make_move_iterator(src_ + 1),
				dst_);
			src_->~T();
		}
	};

} /* namespace FN */
