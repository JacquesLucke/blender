#pragma once

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"
#include "BLI_math_bits.h"

#define SLOT_EMPTY -1
#define SLOT_DUMMY -2
#define LOAD_FACTOR 0.6f
#define PERTURB_SHIFT 5

#define ITER_SLOTS(KEY, SLOT, STATE) \
	uint32_t SLOT, SLOT##_perturb; \
	Index STATE; \
	for (this->first_slot(KEY, &SLOT, &SLOT##_perturb), STATE = m_map[SLOT];; \
		 this->next_slot(&SLOT, &SLOT##_perturb), STATE = m_map[SLOT])

namespace BLI {

	template<typename T>
	inline const T &get_key_from_item(const T &item)
	{
		return item;
	}

	template<
		typename Key,
		typename Item = Key,
		const Key &GetKey(const Item &entry) = get_key_from_item,
		uint N_EXP = 3,
		typename Hash = std::hash<Key>,
		typename Index = int>
	class ArrayLookup {
	private:
		using Mapping = SmallVector<Index, (1 << N_EXP)>;
		Mapping m_map;
		uint m_usable_slots;
		uint m_length;
		uint32_t m_slot_mask;

	public:
		ArrayLookup()
		{
			this->reset_map(1 << N_EXP);
			m_length = 0;
		}

		bool contains(Item *array, const Key &key) const
		{
			ITER_SLOTS(key, slot, state) {
				if (state == SLOT_EMPTY) {
					return false;
				}
				else if (state == SLOT_DUMMY) {
					continue;
				}
				else if (GetKey(array[state]) == key) {
					return true;
				}
			}
		}

		bool add(Item *array, const Key &key, Index potential_index)
		{
			ITER_SLOTS(key, slot, state) {
				if (state == SLOT_EMPTY) {
					bool map_changed = this->ensure_can_add(array);
					if (map_changed) {
						this->insert_index_for_key(key, potential_index);
					}
					else {
						m_map[slot] = potential_index;
					}
					m_length++;
					m_usable_slots--;
					return true;
				}
				else if (state == SLOT_DUMMY) {
					continue;
				}
				else if (GetKey(array[state]) == key) {
					return false;
				}
			}
		}

		void add_new(Item *array, Index index)
		{
			this->ensure_can_add(array);
			const Key &key = GetKey(array[index]);
			this->insert_index_for_key(key, index);
			m_usable_slots--;
			m_length++;
		}

		void remove(const Key &key, Index index)
		{
			ITER_SLOTS(key, slot, state) {
				if (state == index) {
					m_map[slot] = SLOT_DUMMY;
					m_length--;
					break;
				}
			}
		}

		void update_index(const Key &key, Index old_index, Index new_index)
		{
			ITER_SLOTS(key, slot, state) {
				if (state == old_index) {
					m_map[slot] = new_index;
					break;
				}
			}
		}

		Index find(Item *array, const Key &key) const
		{
			ITER_SLOTS(key, slot, state) {
				if (state == SLOT_EMPTY) {
					return -1;
				}
				else if (state == SLOT_DUMMY) {
					continue;
				}
				else if (GetKey(array[state]) == key) {
					return state;
				}
			}
		}

	private:
		inline bool ensure_can_add(Item *array)
		{
			if (LIKELY(m_usable_slots > 0)) {
				return false;
			}

			this->reset_map(m_map.size() * 2);
			for (uint i = 0; i < m_length; i++) {
				const Key &key = GetKey(array[i]);
				this->insert_index_for_key(key, i);
			}
			m_usable_slots -= m_length;
			return true;
		}

		void reset_map(uint size)
		{
			BLI_assert(count_bits_i(size) == 1);
			m_map = Mapping(size);
			m_map.fill(SLOT_EMPTY);
			m_usable_slots = m_map.size() * LOAD_FACTOR;
			m_slot_mask = size - 1;
		}

		inline void insert_index_for_key(const Key &key, Index index)
		{
			ITER_SLOTS(key, slot, state) {
				if (state == SLOT_EMPTY) {
					m_map[slot] = index;
					break;
				}
			}
		}

		inline void first_slot(
			const Key &key,
			uint32_t *slot,
			uint32_t *perturb) const
		{
			uint32_t hash_value = Hash{}(key);
			*slot = hash_value & m_slot_mask;
			*perturb = hash_value;
		}

		inline void next_slot(
			uint32_t *slot,
			uint32_t *perturb) const
		{
			*slot = m_slot_mask & ((5 + *slot) + 1 + *perturb);
			*perturb >>= PERTURB_SHIFT;
		}
	};

} /* namespace BLI */

#undef SLOT_EMPTY
#undef SLOT_DUMMY
#undef LOAD_FACTOR
#undef PERTURB_SHIFT
