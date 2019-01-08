#pragma once

#include <vector>
#include <iostream>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include "ArraySet.hpp"
#include "HashMap.hpp"

/* copied from BLI_utildefines.h */
#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

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
	const Node *node() const;
	uint index() const;

	Type *type() const;
	std::string debug_name() const;
	std::string str_id() const;

	static AnySocket NewInput(const Node *node, uint index);
	static AnySocket NewOutput(const Node *node, uint index);

	friend bool operator==(const AnySocket &left, const AnySocket &right);
	friend std::ostream &operator<<(std::ostream &stream, const AnySocket &socket);

private:
	AnySocket(const Node *node, bool is_output, uint index);

	const SocketInfo *info() const;

	const Node *m_node;
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
	virtual ~Node();

	const NodeSockets &inputs() const;
	const NodeSockets &outputs() const;

	AnySocket Input(const uint index) const;
	AnySocket Output(const uint index) const;

	void addInput(std::string debug_name, Type *type);
	void addOutput(std::string debug_name, Type *type);

	std::string str_id() const;
	virtual std::string debug_name() const;

	virtual void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const = 0;

	friend std::ostream &operator<<(std::ostream &stream, const Node &node);

private:
	NodeSockets m_inputs, m_outputs;
};

struct Link {
	AnySocket from, to;

	Link(AnySocket from, AnySocket to);
};

struct LinkSet {
	std::vector<Link> links;

	bool isLinked(AnySocket socket) const;
	AnySocket getOriginSocket(AnySocket socket) const;
	SocketSet getTargetSockets(AnySocket socket) const;
};

class CompiledFunction {
public:
	CompiledFunction(void *function_pointer);
	virtual ~CompiledFunction();
	void *pointer();

private:
	void *function_pointer;
};

class CompiledLLVMFunction : public CompiledFunction {
public:
	CompiledLLVMFunction(
		llvm::ExecutionEngine *ee,
		llvm::Module *module,
		std::string function_name);
	~CompiledLLVMFunction();

	void printCode();

private:
	llvm::ExecutionEngine *ee;
	llvm::Module *module;
};

using NodeSet = ArraySet<Node *>;

class DataFlowGraph {
public:
	~DataFlowGraph();

	void addNode(Node *node);
	void addLink(AnySocket from, AnySocket to);

	bool verify() const;

	const NodeSet &nodes() const;
	const LinkSet &links() const;

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

	NodeSet m_nodes;
	LinkSet m_links;
};

CompiledLLVMFunction *compileDataFlow(
	DataFlowGraph &graph,
	SocketArraySet &inputs,
	SocketArraySet &outputs);

} /* namespace LLVMNodeCompiler */