#pragma once

#include "FN_core.hpp"

struct bNode;
struct bNodeLink;
struct bNodeTree;
struct bNodeSocket;
struct ID;

namespace FN { namespace DataFlowNodes {

	using SocketMap = SmallMap<struct bNodeSocket *, Socket>;

	class Builder {
	private:
		SharedDataFlowGraph &m_graph;
		SocketMap &m_socket_map;

	public:
		Builder(
			SharedDataFlowGraph &graph,
			SocketMap &socket_map)
			: m_graph(graph), m_socket_map(socket_map) {}

		Node *insert_function(SharedFunction &function);
		void insert_link(Socket a, Socket b);

		void map_socket(Socket socket, struct bNodeSocket *bsocket);
		void map_sockets(Node *node, struct bNode *bnode);
		void map_input(Socket socket, struct bNode *bnode, uint index);
		void map_output(Socket socket, struct bNode *bnode, uint index);
	};

	class BuilderContext {
	private:
		struct bNodeTree *m_btree;

	public:
		BuilderContext(struct bNodeTree *btree)
			: m_btree(btree) {}

		bNodeTree *btree() const;
		ID *btree_id() const;

		SharedType &type_of_socket(bNodeSocket *bsocket) const;
	};

} }