#include "BLI_listbase.h"
#include "DNA_listBase.h"

namespace BLI {

	template<typename T, bool intrusive>
	class ListBaseWrapper {
	private:
		ListBase *m_listbase;

	public:
		ListBaseWrapper(ListBase *listbase)
			: m_listbase(listbase) {}

		class Iterator {
		private:
			ListBase *m_listbase;
			Link *m_current;

		public:
			Iterator(ListBase *listbase, Link *current)
				: m_listbase(listbase), m_current(current) {}

			Iterator &operator++()
			{
				m_current = m_current->next;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator iterator = *this;
				++*this;
				return iterator;
			}

			bool operator!=(const Iterator &iterator) const
			{
				return m_current != iterator.m_current;
			}

			T *operator*() const
			{
				if (intrusive) {
					return (T *)m_current;
				}
				else {
					return (T *)((LinkData *)m_current)->data;
				}
			}

		};

		Iterator begin() const
		{
			return Iterator(m_listbase, (Link *)m_listbase->first);
		}

		Iterator end() const
		{
			return Iterator(m_listbase, nullptr);
		}

	};

} /* namespace BLI */