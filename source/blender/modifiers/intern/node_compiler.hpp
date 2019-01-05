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

struct NodeSockets {
private:
	using sockets_t = std::vector<SocketInfo>;
	sockets_t sockets;

public:
	using const_iterator = typename sockets_t::const_iterator;

	NodeSockets() {}

	inline void add(SocketInfo socket)
	{ this->sockets.push_back(socket); }

	inline void add(std::string debug_name, Type *type)
	{ this->sockets.push_back(SocketInfo(debug_name, type)); }

	inline uint size() const
	{ return this->sockets.size(); }

	const SocketInfo &operator[](const int index) const
	{ return this->sockets[index]; }

	const_iterator begin() const
	{ return this->sockets.begin(); }
	const_iterator end() const
	{ return this->sockets.end(); }
};

class Node {
protected:
	NodeSockets m_inputs, m_outputs;
public:
	inline const NodeSockets &inputs()
	{ return this->m_inputs; }
	inline const NodeSockets &outputs()
	{ return this->m_outputs; }

	virtual std::string debug_id() const;

	virtual void buildLLVMIR(
		std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder) = 0;

	inline AnySocket Input(const uint index)
	{ return AnySocket::NewInput(this, index); }
	inline AnySocket Output(const uint index)
	{ return AnySocket::NewOutput(this, index); }
};

class SingleBuilderNode : public Node {
	virtual void buildLLVMIR(
		llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) = 0;

	void buildLLVMIR(
		std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)
	{
		this->buildLLVMIR(builder, inputs, r_outputs);
		*r_builder = builder;
	}
};

llvm::CallInst *callPointer(
	llvm::IRBuilder<> &builder,
	void *pointer, llvm::FunctionType *type, llvm::ArrayRef<llvm::Value *> arguments);

class ExecuteFunctionNode : public Node {
	virtual void *getExecuteFunction() = 0;

	void buildLLVMIR(
		std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)
	{
		llvm::LLVMContext &context = builder->getContext();

		std::vector<llvm::Type *> arg_types;
		for (auto socket : this->inputs()) {
			arg_types.push_back(socket.type->getLLVMType(context));
		}
		std::vector<llvm::Value *> arguments = inputs;
		std::vector<llvm::Value *> output_pointers;
		for (auto socket : this->outputs()) {
			llvm::Type *type = socket.type->getLLVMType(context);
			arg_types.push_back(type->getPointerTo());
			llvm::Value *alloced_ptr = builder->CreateAlloca(type);
			output_pointers.push_back(alloced_ptr);
			arguments.push_back(alloced_ptr);
		}

		llvm::FunctionType *ftype = llvm::FunctionType::get(
			llvm::Type::getVoidTy(context), arg_types, false);
		callPointer(*builder, this->getExecuteFunction(), ftype, arguments);

		for (auto output_pointer : output_pointers) {
			llvm::Value *result = builder->CreateLoad(output_pointer);
			r_outputs.push_back(result);
		}

		*r_builder = builder;
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
