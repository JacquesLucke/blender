#include "data_flow_graph.hpp"

namespace FN {
	SharedType &Socket::type() const
	{
		if (m_is_output) {
			return this->node()->signature().outputs()[m_index].type();
		}
		else {
			return this->node()->signature().inputs()[m_index].type();
		}
	}

	std::string Socket::name() const
	{
		if (m_is_output) {
			return this->node()->signature().outputs()[m_index].name();
		}
		else {
			return this->node()->signature().inputs()[m_index].name();
		}
	}


	DataFlowGraph::DataFlowGraph()
	{
		m_node_pool = new MemPool(sizeof(Node));
	}

	DataFlowGraph::~DataFlowGraph()
	{
		for (Node *node : m_nodes) {
			node->~Node();
		}
		delete m_node_pool;
	}

	Node *DataFlowGraph::insert(SharedFunction &function, SourceInfo *source)
	{
		BLI_assert(this->can_modify());

		void *ptr = m_node_pool->allocate();
		Node *node = new(ptr) Node(this, function, source);
		m_nodes.add(node);
		return node;
	}

	void DataFlowGraph::link(Socket a, Socket b)
	{
		BLI_assert(this->can_modify());
		BLI_assert(a.node() != b.node());
		BLI_assert(a.type() == b.type());
		BLI_assert(a.is_input() != b.is_input());
		BLI_assert(a.graph() == this && b.graph() == this);

		m_links.insert(Link::New(a, b));
	}


	SocketSet FunctionGraph::find_required_sockets() const
	{
		SocketSet found;

		SocketSet to_be_checked;
		for (Socket socket : m_outputs) {
			to_be_checked.add(socket);
		}

		while (to_be_checked.size() > 0) {
			Socket socket = to_be_checked.pop();

			if (m_inputs.contains(socket)) {
				continue;
			}

			found.add(socket);

			if (socket.is_input()) {
				to_be_checked.add(socket.origin());
			}
			else {
				for (Socket input : socket.node()->inputs()) {
					to_be_checked.add(input);
				}
			}
		}

		return found;
	}

};