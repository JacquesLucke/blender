#pragma once

#include "BLI_small_map.hpp"
#include "BLI_mempool.hpp"

namespace BLI {

	class MemMultiPool {
	private:
		SmallMap<uint, MemPool *> m_pools;

	public:
		MemMultiPool() = default;
		MemMultiPool(MemMultiPool &other) = delete;

		~MemMultiPool()
		{
			for (MemPool *pool : m_pools.values()) {
				delete pool;
			}
		}

		template<typename T>
		T *allocate()
		{
			return (T *)this->allocate(sizeof(T));
		}

		template<typename T>
		T *allocate_array(uint length)
		{
			return (T *)this->allocate(sizeof(T) * length);
		}

		void *allocate(uint size)
		{
			uint alloc_size = size + sizeof(uint);
			MemPool *pool = m_pools.lookup_default(alloc_size, NULL);

			if (pool == NULL) {
				pool = new MemPool(alloc_size);
				m_pools.add(alloc_size, pool);
			}

			void *real_ptr = pool->allocate();
			*(uint *)real_ptr = alloc_size;

			void *ptr = (void *)((uint *)real_ptr + 1);
			return ptr;
		}

		void deallocate(void *ptr)
		{
			void *real_ptr = (uint *)ptr - 1;
			uint alloc_size = *(uint *)real_ptr;
			BLI_assert(m_pools.contains(alloc_size));

			MemPool *pool = m_pools.lookup(alloc_size);
			pool->deallocate(ptr);
		}
	};

} /* namespace BLI */
