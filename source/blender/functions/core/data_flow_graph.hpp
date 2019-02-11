#pragma once

#include "core.hpp"

#include "BLI_small_set.hpp"
#include "BLI_small_set_vector.hpp"

namespace FN {

	class Socket;
	class Node;
	class GraphLinks;
	class DataFlowGraph;

	class Socket {
	public:
		static inline Socket Input(const Node *node, uint index);
		static inline Socket Output(const Node *node, uint index);

		inline const Node *node() const;
		inline DataFlowGraph *graph() const;

		inline bool is_input() const;
		inline bool is_output() const;
		inline uint index() const;

		const SharedType &type() const;
		std::string name() const;

		friend bool operator==(const Socket &a, const Socket &b);

		inline Socket origin() const;
		inline bool is_linked() const;

	private:
		Socket(const Node *node, bool is_output, uint index)
			: m_node(node), m_is_output(is_output), m_index(index) {}

		const Node *m_node;
		bool m_is_output;
		uint m_index;
	};


	using SmallSocketVector = SmallVector<Socket>;
	using SmallSocketSetVector = SmallSetVector<Socket>;

	class Node {
	public:
		Node(DataFlowGraph *graph, SharedFunction &function)
			: m_graph(graph), m_function(function) {}

		Socket input(uint index) const
		{
			return Socket::Input(this, index);
		}

		Socket output(uint index) const
		{
			return Socket::Output(this, index);
		}

		DataFlowGraph *graph() const
		{
			return m_graph;
		}

		const SharedFunction &function() const
		{
			return m_function;
		}

		const Signature &signature() const
		{
			return this->function()->signature();
		}

	private:
		DataFlowGraph *m_graph;
		SharedFunction m_function;
	};

	class Link {
	public:
		static Link New(Socket a, Socket b)
		{
			BLI_assert(a.is_input() != b.is_input());
			if (a.is_input()) {
				return Link(b, a);
			}
			else {
				return Link(a, b);
			}
		}

		Socket from() const
		{
			return m_from;
		}

		Socket to() const
		{
			return m_to;
		}

		friend bool operator==(const Link &a, const Link &b)
		{
			return a.m_from == b.m_from && a.m_to == b.m_to;
		}

	private:
		Link(Socket from, Socket to)
			: m_from(from), m_to(to) {}

		Socket m_from;
		Socket m_to;
	};

	class GraphLinks {
	public:
		void insert(Link link)
		{
			Socket from = link.from();
			Socket to = link.to();

			if (!m_links.contains(from)) {
				m_links.add(from, SmallSet<Socket>());
			}
			if (!m_links.contains(to)) {
				m_links.add(to, SmallSet<Socket>());
			}

			m_links.lookup_ref(from).add(to);
			m_links.lookup_ref(to).add(from);
			m_all_links.append(Link::New(from, to));
		}

		SmallSet<Socket> get_linked(Socket socket) const
		{
			SmallSet<Socket> *linked = m_links.lookup_ptr(socket);
			if (linked == nullptr) return {};
			return *linked;
		}

		SmallVector<Link> all_links() const
		{
			return m_all_links;
		}

		Socket get_origin(Socket socket) const
		{
			BLI_assert(socket.is_input());
			auto linked = this->get_linked(socket);
			BLI_assert(linked.size() == 1);
			return linked.any();
		}

	private:
		SmallMap<Socket, SmallSet<Socket>> m_links;
		SmallVector<Link> m_all_links;
	};

	class DataFlowGraph {
	public:
		DataFlowGraph() = default;

		~DataFlowGraph()
		{
			for (const Node *node : m_nodes) {
				delete node;
			}
		}

		const Node *insert(SharedFunction &function)
		{
			BLI_assert(this->can_modify());
			const Node *node = new Node(this, function);
			m_nodes.add(node);
			return node;
		}

		void link(Socket a, Socket b)
		{
			BLI_assert(this->can_modify());
			BLI_assert(a.node() != b.node());
			BLI_assert(a.is_input() != b.is_input());
			BLI_assert(a.graph() == this && b.graph() == this);

			m_links.insert(Link::New(a, b));
		}

		inline bool can_modify() const
		{
			return !this->frozen();
		}

		inline bool frozen() const
		{
			return m_frozen;
		}

		void freeze()
		{
			m_frozen = true;
		}

		SmallVector<Link> all_links() const
		{
			return m_links.all_links();
		}

		const SmallSet<const Node *> &all_nodes() const
		{
			return m_nodes;
		}

		std::string to_dot() const;

	private:
		bool m_frozen = false;
		SmallSet<const Node *> m_nodes;
		GraphLinks m_links;

		friend Node;
		friend Socket;
	};

	using SharedDataFlowGraph = Shared<DataFlowGraph>;

	class FunctionGraph {
	public:
		FunctionGraph(
			const SharedDataFlowGraph &graph,
			const SmallSocketVector &inputs,
			const SmallSocketVector &outputs)
			: m_graph(graph), m_inputs(inputs), m_outputs(outputs)
		{
			BLI_assert(graph->frozen());
		}

		const SharedDataFlowGraph &graph() const
		{
			return m_graph;
		}

		const SmallSocketVector &inputs() const
		{
			return m_inputs;
		}

		const SmallSocketVector &outputs() const
		{
			return m_outputs;
		}

		Signature signature() const
		{
			InputParameters inputs;
			OutputParameters outputs;

			for (const Socket &socket : m_inputs) {
				inputs.append(InputParameter(socket.name(), socket.type()));
			}
			for (const Socket &socket : m_outputs) {
				outputs.append(OutputParameter(socket.name(), socket.type()));
			}

			return Signature(inputs, outputs);
		}

	private:
		SharedDataFlowGraph m_graph;
		SmallSocketVector m_inputs;
		SmallSocketVector m_outputs;
	};


	/* Socket Inline Functions
	 ********************************************** */

	inline Socket Socket::Input(const Node *node, uint index)
	{
		BLI_assert(index < node->signature().inputs().size());
		return Socket(node, false, index);
	}

	inline Socket Socket::Output(const Node *node, uint index)
	{
		BLI_assert(index < node->signature().outputs().size());
		return Socket(node, true, index);
	}

	const Node *Socket::node() const
	{
		return m_node;
	}

	DataFlowGraph *Socket::graph() const
	{
		return this->node()->graph();
	}

	bool Socket::is_input() const
	{
		return !m_is_output;
	}

	bool Socket::is_output() const
	{
		return m_is_output;
	}

	uint Socket::index() const
	{
		return m_index;
	}

	inline bool operator==(const Socket &a, const Socket &b)
	{
		return (
			a.m_node == b.m_node &&
			a.m_is_output == b.m_is_output &&
			a.m_index == b.m_index);
	}

	Socket Socket::origin() const
	{
		return this->graph()->m_links.get_origin(*this);
	}

	bool Socket::is_linked() const
	{
		return this->graph()->m_links.get_linked(*this).size() > 0;
	}

} /* namespace FN */