#pragma once

#include "FN_core.hpp"
#include "./util_wrappers.hpp"

struct bNode;
struct bNodeLink;
struct bNodeTree;
struct bNodeSocket;
struct ID;
struct PointerRNA;

namespace FN { namespace DataFlowNodes {

	using SocketMap = SmallMap<struct bNodeSocket *, Socket>;

	class BuilderContext {
	private:
		struct bNodeTree *m_btree;

	public:
		BuilderContext(struct bNodeTree *btree)
			: m_btree(btree) {}

		bNodeTree *btree() const;
		ID *btree_id() const;

		bool is_data_socket(bNodeSocket *bsocket) const;
		SharedType &type_by_name(const char *data_type) const;
		SharedType &type_of_socket(bNodeSocket *bsocket) const;
		std::string name_of_socket(bNode *bnode, bNodeSocket *bsocket) const;

		void get_rna(bNode *bnode, PointerRNA *ptr) const;
		void get_rna(bNodeSocket *bsocket, PointerRNA *ptr) const;
		SharedType &type_from_rna(bNode *bnode, const char *prop_name) const;
		std::string socket_type_string(bNodeSocket *bsocket) const;
	};

	class Builder {
	private:
		const BuilderContext &m_ctx;
		SharedDataFlowGraph &m_graph;
		SocketMap &m_socket_map;

	public:
		Builder(
			const BuilderContext &ctx,
			SharedDataFlowGraph &graph,
			SocketMap &socket_map)
			: m_ctx(ctx), m_graph(graph), m_socket_map(socket_map) {}

		Node *insert_function(SharedFunction &fn);
		Node *insert_function(SharedFunction &fn, struct bNode *bnode);
		Node *insert_function(SharedFunction &fn, struct bNodeLink *blink);
		void insert_link(Socket a, Socket b);

		void map_socket(Socket socket, struct bNodeSocket *bsocket);
		void map_sockets(Node *node, struct bNode *bnode);
		void map_data_sockets(Node *node, struct bNode *bnode);
		void map_input(Socket socket, struct bNode *bnode, uint index);
		void map_output(Socket socket, struct bNode *bnode, uint index);

		Socket lookup_socket(struct bNodeSocket *bsocket);
		bool verify_data_sockets_mapped(struct bNode *bnode) const;

		const BuilderContext &ctx() const
		{
			return m_ctx;
		}

	private:
		bool check_if_sockets_are_mapped(
			struct bNode *bnode,
			bSocketList bsockets) const;
	};

} }