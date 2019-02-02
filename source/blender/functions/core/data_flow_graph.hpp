#pragma once

#include "core.hpp"

#include "BLI_small_set.hpp"

namespace FN {

	class Socket;
	class Node;
	class GraphLinks;
	class DataFlowGraph;

	class Socket {
	public:
		static Socket Input(const Node *node, uint index)
		{
			return Socket(node, false, index);
		}

		static Socket Output(const Node *node, uint index)
		{
			return Socket(node, true, index);
		}

		const Node *node() const
		{
			return this->m_node;
		}

		bool is_input() const
		{
			return !this->m_is_output;
		}

		bool is_output() const
		{
			return this->m_is_output;
		}

		uint index() const
		{
			return this->m_index;
		}

		const Type *type() const;

		friend bool operator==(const Socket &a, const Socket &b)
		{
			return (
				a.m_node == b.m_node &&
				a.m_is_output == b.m_is_output &&
				a.m_index == b.m_index);
		}

	private:
		Socket(const Node *node, bool is_output, uint index)
			: m_node(node), m_is_output(is_output), m_index(index) {}

		const Node *m_node;
		const bool m_is_output;
		const uint m_index;
	};

	class Node {
	public:
		Node(const Function &function)
			: m_function(function) {}

		Socket input(uint index) const
		{
			return Socket::Input(this, index);
		}

		Socket output(uint index) const
		{
			return Socket::Output(this, index);
		}

		const Function &function() const
		{
			return this->m_function;
		}

		const Signature &signature() const
		{
			return this->function().signature();
		}

	private:
		const Function &m_function;
	};

	class GraphLinks {
	public:
		void insert(Socket a, Socket b)
		{
			if (!this->m_links.contains(a)) {
				this->m_links.add(a, SmallSet<Socket>());
			}
			if (!this->m_links.contains(b)) {
				this->m_links.add(b, SmallSet<Socket>());
			}

			this->m_links.lookup_ref(a).add(b);
			this->m_links.lookup_ref(b).add(a);
		}

		SmallSet<Socket> get_linked(Socket socket) const
		{
			return this->m_links.lookup(socket);
		}

	private:
		SmallMap<Socket, SmallSet<Socket>> m_links;
	};

	class DataFlowGraph {
	public:
		DataFlowGraph() = default;

		~DataFlowGraph()
		{
			for (const Node *node : this->m_nodes) {
				delete node;
			}
		}

		const Node *insert(const Function &function)
		{
			BLI_assert(this->can_modify());
			const Node *node = new Node(function);
			this->m_nodes.add(node);
			return node;
		}

		void link(Socket a, Socket b)
		{
			BLI_assert(this->can_modify());
			BLI_assert(a.node() != b.node());
			BLI_assert(a.is_input() != b.is_input());
			BLI_assert(m_nodes.contains(a.node()));
			BLI_assert(m_nodes.contains(b.node()));

			m_links.insert(a, b);
			m_links.insert(b, a);
		}

		inline bool can_modify() const
		{
			return !this->frozen();
		}

		inline bool frozen() const
		{
			return this->m_frozen;
		}

		void freeze()
		{
			this->m_frozen = true;
		}

	private:
		bool m_frozen = false;
		SmallSet<const Node *> m_nodes;
		GraphLinks m_links;
	};

} /* namespace FN */