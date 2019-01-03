#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

#include "HashSet.hpp"

namespace NodeCompiler {

struct AnySocket;
struct SocketInfo;
struct SimpleNode;
struct Link;
struct LinkSet;
struct Graph;

struct AnySocket {
	inline bool is_output() const { return this->_is_output; }
	inline bool is_input() const { return !this->_is_output; }
	inline SimpleNode *node() const { return this->_node; }
	inline uint index() const { return this->_index; }

	const llvm::Type *type() const;
	const std::string &debug_name() const;

	inline static AnySocket NewInput(SimpleNode *node, uint index)
	{ return AnySocket(node, false, index); }

	inline static AnySocket NewOutput(SimpleNode *node, uint index)
	{ return AnySocket(node, true, index); }

	friend bool operator==(const AnySocket &left, const AnySocket &right)
	{
		return (
			   left._node == right._node
			&& left._is_output == right._is_output
			&& left._index == right._index);
	}

private:
	AnySocket(SimpleNode *node, bool is_output, uint index)
		: _node(node), _is_output(is_output), _index(index) {}

	const SocketInfo *info() const;

	SimpleNode *_node;
	bool _is_output;
	uint _index;
};

using SocketSet = HashSet<AnySocket>;

struct SocketInfo {
	const std::string debug_name;
	const llvm::Type *type;

	SocketInfo(std::string debug_name, llvm::Type *type)
		: debug_name(debug_name), type(type) {}
};

struct SimpleNode {
	std::string debug_name;
	std::vector<SocketInfo> inputs;
	std::vector<SocketInfo> outputs;
	std::function<void(
		std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)> generateCode;

	std::string debug_id() const;

	inline AnySocket Input(uint index)
	{ return AnySocket::NewInput(this, index); }
	inline AnySocket Output(uint index)
	{ return AnySocket::NewOutput(this, index); }
};

struct Link {
	AnySocket from, to;

	Link(AnySocket from, AnySocket to)
		: from(from), to(to) {}
};

struct LinkSet {
	std::vector<Link> links;

	AnySocket getOriginSocket(AnySocket socket) const;
};

struct AnySocketHash {
	size_t operator()(const AnySocket &socket) const
	{
		return (size_t)socket.node() ^ (size_t)socket.is_input() ^ (size_t)socket.index();
	}
};

struct Graph {
	std::vector<SimpleNode *> nodes;
	LinkSet links;

	void generateCode(
		llvm::IRBuilder<> &builder,
		std::vector<AnySocket> &inputs, std::vector<AnySocket> &outputs, std::vector<llvm::Value *> &input_values,
		llvm::IRBuilder<> *r_builder, std::vector<llvm::Value *> *r_output_values);

	AnySocket getOriginSocket(AnySocket socket) const;

	std::string toDotFormat(std::vector<SimpleNode *> marked_nodes = {}) const;

	SocketSet findRequiredSockets(SocketSet &inputs, SocketSet &outputs);
private:
	void findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets);
};

} /* namespace NodeCompiler */
