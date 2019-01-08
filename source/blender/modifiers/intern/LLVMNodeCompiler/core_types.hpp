#pragma once

#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include "ArraySet.hpp"
#include "HashMap.hpp"

#include "BLI_utildefines.h"

namespace LLVMNodeCompiler {

struct AnySocket;
struct SocketInfo;
struct Node;
struct Link;
struct Type;
struct LinkSet;
struct DataFlowGraph;

class Type {
public:
	llvm::Type *getLLVMType(llvm::LLVMContext &context);

	virtual llvm::Value *buildCopyIR(llvm::IRBuilder<> &builder, llvm::Value *value);
	virtual void buildFreeIR(llvm::IRBuilder<> &builder, llvm::Value *value);

private:
	HashMap<llvm::LLVMContext *, llvm::Type *> typePerContext;

	/* Will be called at most once for every context. */
	virtual llvm::Type *createLLVMType(llvm::LLVMContext &context) = 0;
};

struct AnySocket {
public:
	bool is_output() const;
	bool is_input() const;
	Node *node() const;
	uint index() const;

	Type *type() const;
	std::string debug_name() const;
	std::string str_id() const;

	static AnySocket NewInput(Node *node, uint index);
	static AnySocket NewOutput(Node *node, uint index);

	friend bool operator==(const AnySocket &left, const AnySocket &right);

private:
	AnySocket(Node *node, bool is_output, uint index);

	const SocketInfo *info() const;

	Node *m_node;
	bool m_is_output;
	uint m_index;
};

using SocketArraySet = ArraySet<AnySocket>;
using SocketSet = SocketArraySet;

template<typename TValue>
using SocketMap = HashMap<AnySocket, TValue>;

using SocketValueMap = SocketMap<llvm::Value *>;

struct SocketInfo {
	std::string debug_name;
	Type *type;

	SocketInfo(std::string debug_name, Type *type);
};

struct NodeSockets {
private:
	using sockets_t = std::vector<SocketInfo>;
	using const_iterator = typename sockets_t::const_iterator;

public:
	NodeSockets() {}

	void add(SocketInfo socket);
	void add(std::string debug_name, Type *type);

	uint size() const;

	const SocketInfo &operator[](const int index) const;

	const_iterator begin() const;
	const_iterator end() const;

private:
	sockets_t sockets;
};

class Node {
public:
	const NodeSockets &inputs();
	const NodeSockets &outputs();

	AnySocket Input(const uint index);
	AnySocket Output(const uint index);

	void addInput(std::string debug_name, Type *type);
	void addOutput(std::string debug_name, Type *type);

	std::string str_id() const;
	virtual std::string debug_name() const;

	virtual void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) = 0;

private:
	NodeSockets m_inputs, m_outputs;
};

struct Link {
	AnySocket from, to;

	Link(AnySocket from, AnySocket to);
};

struct LinkSet {
	std::vector<Link> links;

	AnySocket getOriginSocket(AnySocket socket) const;
	SocketSet getTargetSockets(AnySocket socket) const;
};

class DataFlowCallable {
public:
	DataFlowCallable(llvm::Module *module, llvm::ExecutionEngine *ee, std::string function_name);
	void *getFunctionPointer();
	void printCode();

private:
	void *function_pointer;
	llvm::Module *module;
	llvm::ExecutionEngine *ee;
};

class DataFlowGraph {
public:
	void addNode(Node *node);
	void addLink(AnySocket from, AnySocket to);

	DataFlowCallable *generateCallable(
		std::string debug_name,
		SocketArraySet &inputs, SocketArraySet &outputs);

	llvm::Module *generateModule(
		llvm::LLVMContext &context,
		std::string module_name, std::string function_name,
		SocketArraySet &inputs, SocketArraySet &outputs);

	llvm::Function *generateFunction(
		llvm::Module *module, std::string name,
		SocketArraySet &inputs, SocketArraySet &outputs);

	void generateCode(
		llvm::IRBuilder<> &builder,
		SocketArraySet &inputs, SocketArraySet &outputs, std::vector<llvm::Value *> &input_values,
		std::vector<llvm::Value *> &r_output_values);

	AnySocket getOriginSocket(AnySocket socket) const;
	SocketSet getTargetSockets(AnySocket socket) const;

	std::string toDotFormat(std::vector<Node *> marked_nodes = {}) const;

	SocketSet findRequiredSockets(SocketSet &inputs, SocketSet &outputs);

private:
	void findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets);

	void generateCodeForSocket(
		llvm::IRBuilder<> &builder,
		AnySocket socket,
		SocketValueMap &values,
		SocketSet &required_sockets,
		SocketSet &forwarded_sockets);

	void forwardOutputIfNecessary(
		llvm::IRBuilder<> &builder,
		AnySocket output,
		SocketValueMap &values,
		SocketSet &required_sockets,
		SocketSet &forwarded_sockets);

	void forwardOutput(
		llvm::IRBuilder<> &builder,
		AnySocket output,
		SocketValueMap &values,
		SocketSet &required_sockets);

	std::vector<Node *> nodes;
	LinkSet links;
};

} /* namespace LLVMNodeCompiler */