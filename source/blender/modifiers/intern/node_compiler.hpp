#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

#include "ArraySet.hpp"
#include "HashMap.hpp"

namespace LLVMNodeCompiler {

struct AnySocket;
struct SocketInfo;
struct Node;
struct Link;
struct Type;
struct LinkSet;
struct DataFlowGraph;

class Type {
private:
	HashMap<llvm::LLVMContext *, llvm::Type *> typePerContext;

	/* Will be called at most once for every context. */
	virtual llvm::Type *createLLVMType(llvm::LLVMContext &context) = 0;

public:
	llvm::Type *getLLVMType(llvm::LLVMContext &context);

	// virtual llvm::Value *buildCopyIR(llvm::Value *value);
	// virtual void buildFreeIR(llvm::Value *value);
};

struct AnySocket {
	inline bool is_output() const { return this->_is_output; }
	inline bool is_input() const { return !this->_is_output; }
	inline Node *node() const { return this->_node; }
	inline uint index() const { return this->_index; }

	Type *type() const;
	std::string debug_name() const;

	inline static AnySocket NewInput(Node *node, uint index)
	{ return AnySocket(node, false, index); }

	inline static AnySocket NewOutput(Node *node, uint index)
	{ return AnySocket(node, true, index); }

	friend bool operator==(const AnySocket &left, const AnySocket &right)
	{
		return (
			   left._node == right._node
			&& left._is_output == right._is_output
			&& left._index == right._index);
	}

private:
	AnySocket(Node *node, bool is_output, uint index)
		: _node(node), _is_output(is_output), _index(index) {}

	const SocketInfo *info() const;

	Node *_node;
	bool _is_output;
	uint _index;
};

using SocketArraySet = ArraySet<AnySocket>;
using SocketSet = SocketArraySet;

template<typename TValue>
using SocketMap = HashMap<AnySocket, TValue>;

using SocketValueMap = SocketMap<llvm::Value *>;

struct SocketInfo {
	std::string debug_name;
	Type *type;

	SocketInfo(std::string debug_name, Type *type)
		: debug_name(debug_name), type(type) {}
};

typedef std::function<void(
	std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
	std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)> IRBuilderFunction;

class Node final {
private:
	std::string debug_name;
	std::vector<SocketInfo> _inputs;
	std::vector<SocketInfo> _outputs;
	IRBuilderFunction generateCode;

public:
	static Node *FromIRBuilderFunction(
		const std::string &debug_name,
		const std::vector<SocketInfo> &inputs,
		const std::vector<SocketInfo> &outputs,
		const IRBuilderFunction &generateCode)
	{
		Node *node = new Node();
		node->debug_name = debug_name;
		node->_inputs = inputs;
		node->_outputs = outputs;
		node->generateCode = generateCode;
		return node;
	}

	std::string debug_id() const;

	inline const std::vector<SocketInfo> &inputs()
	{ return this->_inputs; }
	inline const std::vector<SocketInfo> &outputs()
	{ return this->_outputs; }

	inline AnySocket Input(uint index)
	{ return AnySocket::NewInput(this, index); }
	inline AnySocket Output(uint index)
	{ return AnySocket::NewOutput(this, index); }

	void build_ir(
		std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)
	{
		this->generateCode(inputs, builder, r_outputs, r_builder);
	}
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

class DataFlowCallable {
	void *function_pointer;
	llvm::Module *module;
	llvm::ExecutionEngine *ee;
public:
	DataFlowCallable(llvm::Module *module, llvm::ExecutionEngine *ee, std::string function_name)
		: module(module), ee(ee)
	{
		this->function_pointer = (void *)this->ee->getFunctionAddress(function_name);
	}

	inline void *getFunctionPointer()
	{ return this->function_pointer; }

	void printCode();
};

class DataFlowGraph {
public:
	std::vector<Node *> nodes;
	LinkSet links;

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
		llvm::IRBuilder<> *builder,
		SocketArraySet &inputs, SocketArraySet &outputs, std::vector<llvm::Value *> &input_values,
		llvm::IRBuilder<> **r_builder, std::vector<llvm::Value *> &r_output_values);

	AnySocket getOriginSocket(AnySocket socket) const;

	std::string toDotFormat(std::vector<Node *> marked_nodes = {}) const;

	SocketSet findRequiredSockets(SocketSet &inputs, SocketSet &outputs);
private:
	void findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets);

	llvm::Value *generateCodeForSocket(
		AnySocket socket,
		llvm::IRBuilder<> *builder,
		SocketValueMap &values,
		llvm::IRBuilder<> **r_builder);
};

} /* namespace LLVMNodeCompiler */
