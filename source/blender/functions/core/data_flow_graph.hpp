#pragma once

#include "function.hpp"
#include "source_info.hpp"

#include "BLI_small_set.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_mempool.hpp"

namespace FN {

	class Socket;
	class Node;
	class GraphLinks;
	class DataFlowGraph;

	using SocketSet = SmallSet<Socket>;
	using NodeSet = SmallSet<Node *>;
	using NodeSetVector = SmallSetVector<Node *>;
	using SmallSocketVector = SmallVector<Socket>;
	using SmallSocketSetVector = SmallSetVector<Socket>;

	class Socket {
	public:
		Socket(Node *node, bool is_output, uint index)
			: m_node(node), m_is_output(is_output), m_index(index) {}

		static inline Socket Input(Node *node, uint index);
		static inline Socket Output(Node *node, uint index);

		inline Node *node() const;
		inline DataFlowGraph *graph() const;

		inline bool is_input() const;
		inline bool is_output() const;
		inline uint index() const;

		SharedType &type() const;
		std::string name() const;

		friend bool operator==(const Socket &a, const Socket &b);

		inline Socket origin() const;
		inline SocketSet targets() const;
		inline bool is_linked() const;

	private:
		Node *m_node;
		bool m_is_output;
		uint m_index;
	};

	class Node {
	public:
		Node(
			DataFlowGraph *graph,
			SharedFunction &function,
			SourceInfo *source)
			: m_graph(graph),
			  m_function(function),
			  m_source(source) {}

		~Node()
		{
			if (m_source != nullptr) {
				delete m_source;
			}
		}

		Socket input(uint index)
		{
			return Socket::Input(this, index);
		}

		Socket output(uint index)
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

		SharedFunction &function()
		{
			return m_function;
		}

		const Signature &signature() const
		{
			return this->function()->signature();
		}

		Signature &signature()
		{
			return this->function()->signature();
		}

		uint input_amount() const
		{
			return this->signature().inputs().size();
		}

		uint output_amount() const
		{
			return this->signature().outputs().size();
		}

		SourceInfo *source() const
		{
			return m_source;
		}

		class SocketIterator {
		private:
			Node *m_node;
			bool m_is_output;
			uint m_index;

			SocketIterator(Node *node, bool is_output, uint index)
				: m_node(node), m_is_output(is_output), m_index(index) {}

		public:
			SocketIterator(Node *node, bool is_output)
				: SocketIterator(node, is_output, 0) {}

			using It = SocketIterator;

			It begin() const
			{
				return It(m_node, m_is_output, 0);
			}

			It end() const
			{
				Signature &sig = m_node->signature();
				return It(m_node, m_is_output,
					(m_is_output) ? sig.outputs().size() : sig.inputs().size());
			}

			It &operator++()
			{
				m_index++;
				return *this;
			}

			bool operator!=(const It &other) const
			{
				return m_index != other.m_index;
			}

			Socket operator*() const
			{
				return Socket(m_node, m_is_output, m_index);
			}
		};

		SocketIterator inputs()
		{
			return SocketIterator(this, false);
		}

		SocketIterator outputs()
		{
			return SocketIterator(this, true);
		}

	private:
		DataFlowGraph *m_graph;
		SharedFunction m_function;
		SourceInfo *m_source;
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

	class DataFlowGraph : public RefCountedBase {
	public:
		DataFlowGraph();

		~DataFlowGraph();

		Node *insert(SharedFunction &function, SourceInfo *source = nullptr);
		void link(Socket a, Socket b);

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

		const SmallSet<Node *> &all_nodes() const
		{
			return m_nodes;
		}

		std::string to_dot() const;
		void to_dot__clipboard() const;

	private:
		bool m_frozen = false;
		SmallSet<Node *> m_nodes;
		GraphLinks m_links;
		MemPool *m_node_pool;

		friend Node;
		friend Socket;
	};

	using SharedDataFlowGraph = AutoRefCount<DataFlowGraph>;

	class FunctionGraph {
	public:
		FunctionGraph(
			SharedDataFlowGraph &graph,
			SmallSocketVector &inputs,
			SmallSocketVector &outputs)
			: m_graph(graph), m_inputs(inputs), m_outputs(outputs)
		{
			BLI_assert(graph->frozen());
		}

		const SharedDataFlowGraph &graph() const
		{
			return m_graph;
		}

		const SmallSocketSetVector &inputs() const
		{
			return m_inputs;
		}

		const SmallSocketSetVector &outputs() const
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

		SocketSet find_required_sockets() const;

	private:
		SharedDataFlowGraph m_graph;
		SmallSocketSetVector m_inputs;
		SmallSocketSetVector m_outputs;
	};


	/* Socket Inline Functions
	 ********************************************** */

	inline Socket Socket::Input(Node *node, uint index)
	{
		BLI_assert(index < node->signature().inputs().size());
		return Socket(node, false, index);
	}

	inline Socket Socket::Output(Node *node, uint index)
	{
		BLI_assert(index < node->signature().outputs().size());
		return Socket(node, true, index);
	}

	Node *Socket::node() const
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

	SocketSet Socket::targets() const
	{
		return this->graph()->m_links.get_linked(*this);
	}

	bool Socket::is_linked() const
	{
		return this->graph()->m_links.get_linked(*this).size() > 0;
	}

} /* namespace FN */