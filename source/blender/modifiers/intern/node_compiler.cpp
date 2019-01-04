#include "node_compiler.hpp"

#include <sstream>

namespace LLVMNodeCompiler {

AnySocket LinkSet::getOriginSocket(AnySocket socket) const
{
	assert(socket.is_input());

	for (Link link : this->links) {
		if (link.to == socket) {
			return link.from;
		}
	}

	assert(!"every input socket needs an origin");
}

AnySocket DataFlowGraph::getOriginSocket(AnySocket socket) const
{
	return this->links.getOriginSocket(socket);
}

const SocketInfo *AnySocket::info() const
{
	if (this->is_input()) {
		return &this->node()->inputs()[this->index()];
	}
	else {
		return &this->node()->outputs()[this->index()];
	}
}

llvm::Type *AnySocket::type() const
{
	return this->info()->type;
}

std::string AnySocket::debug_name() const
{
	return this->info()->debug_name;
}

std::string Node::debug_id() const
{
	std::stringstream ss;
	ss << this->debug_name << " at " << (void *)this;
	return ss.str();
}

void DataFlowCallable::printCode()
{
	this->module->print(llvm::outs(), nullptr);
}


DataFlowCallable *DataFlowGraph::generateCallable(
	std::string debug_name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	std::string function_name = debug_name + " Function";

	llvm::Module *module = this->generateModule(
		debug_name + " Module", function_name,
		inputs, outputs);

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module)).create();
	ee->finalizeObject();
	ee->generateCodeForModule(module);

	DataFlowCallable *callable = new DataFlowCallable(module, ee, function_name);
	return callable;
}

llvm::Module *DataFlowGraph::generateModule(
	std::string module_name, std::string function_name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	assert(outputs.size() > 0);
	llvm::LLVMContext &context = outputs[0].type()->getContext();
	llvm::Module *module = new llvm::Module(module_name, context);
	this->generateFunction(module, function_name, inputs, outputs);
	return module;
}

llvm::Function *DataFlowGraph::generateFunction(
	llvm::Module *module, std::string name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	llvm::LLVMContext &context = module->getContext();

	std::vector<llvm::Type *> input_types;
	for (AnySocket socket : inputs.elements()) {
		input_types.push_back(socket.type());
	}

	std::vector<llvm::Type *> output_types;
	for (AnySocket socket : outputs.elements()) {
		output_types.push_back(socket.type());
	}

	llvm::StructType *return_type = llvm::StructType::create(output_types, name + " Output");

	llvm::FunctionType *function_type = llvm::FunctionType::get(
		return_type, input_types, false);

	llvm::Function *function = llvm::Function::Create(
		function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
		name, module);

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
	llvm::IRBuilder<> builder(context);
	builder.SetInsertPoint(bb);

	std::vector<llvm::Value *> input_values;
	for (uint i = 0; i < inputs.size(); i++) {
		input_values.push_back(function->arg_begin() + i);
	}

	std::vector<llvm::Value *> output_values;
	llvm::IRBuilder<> *next_builder;
	this->generateCode(&builder, inputs, outputs, input_values, &next_builder, output_values);

	llvm::Value *output = llvm::UndefValue::get(return_type);
	for (uint i = 0; i < outputs.size(); i++) {
		output = next_builder->CreateInsertValue(output, output_values[i], i);
	}
	next_builder->CreateRet(output);

	llvm::verifyFunction(*function, &llvm::outs());
	llvm::verifyModule(*module, &llvm::outs());

	return function;
}

void DataFlowGraph::generateCode(
	llvm::IRBuilder<> *builder,
	SocketArraySet &inputs, SocketArraySet &outputs, std::vector<llvm::Value *> &input_values,
	llvm::IRBuilder<> **r_builder, std::vector<llvm::Value *> &r_output_values)
{
	assert(inputs.size() == input_values.size());

	SocketValueMap values;
	for (uint i = 0; i < inputs.size(); i++) {
		values.add(inputs[i], input_values[i]);
	}

	for (AnySocket socket : outputs.elements()) {
		llvm::IRBuilder<> *next_builder;

		llvm::Value *value = this->generateCodeForSocket(socket, builder, values, &next_builder);
		r_output_values.push_back(value);

		builder = next_builder;
	}

	*r_builder = builder;
}

llvm::Value *DataFlowGraph::generateCodeForSocket(
	AnySocket socket,
	llvm::IRBuilder<> *builder,
	SocketValueMap &values,
	llvm::IRBuilder<> **r_builder)
{
	if (values.contains(socket)) {
		*r_builder = builder;
		return values.lookup(socket);
	}

	if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		llvm::Value *value = this->generateCodeForSocket(origin, builder, values, r_builder);
		values.add(socket, value);
		return value;
	}

	if (socket.is_output()) {
		Node *node = socket.node();
		std::vector<llvm::Value *> input_values;
		for (uint i = 0; i < node->inputs().size(); i++) {
			llvm::IRBuilder<> *next_builder;

			llvm::Value *value = this->generateCodeForSocket(node->Input(i), builder, values, &next_builder);
			input_values.push_back(value);

			builder = next_builder;
		}

		std::vector<llvm::Value *> output_values;
		node->build_ir(input_values, builder, output_values, r_builder);

		for (uint i = 0; i < node->outputs().size(); i++) {
			values.add(node->Output(i), output_values[i]);
		}

		return values.lookup(socket);
	}

	assert(!"should never happen");
}

SocketSet DataFlowGraph::findRequiredSockets(SocketSet &inputs, SocketSet &outputs)
{
	SocketSet required_sockets;

	for (AnySocket socket : outputs.elements()) {
		this->findRequiredSockets(socket, inputs, required_sockets);
	}

	return required_sockets;
}

void DataFlowGraph::findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets)
{
	if (required_sockets.contains(socket)) {
		return;
	}

	required_sockets.add(socket);

	if (inputs.contains(socket)) {
		return;
	}

	if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		this->findRequiredSockets(origin, inputs, required_sockets);
		return;
	}

	if (socket.is_output()) {
		Node *node = socket.node();
		for (uint i = 0; i < node->inputs().size(); i++) {
			AnySocket input = AnySocket::NewInput(socket.node(), i);
			this->findRequiredSockets(input, inputs, required_sockets);
		}
		return;
	}
}

std::string DataFlowGraph::toDotFormat(std::vector<Node *> marked_nodes) const
{
	std::stringstream ss;
	ss << "digraph MyGraph {" << std::endl;

	for (Node *node : this->nodes) {
		ss << "    \"" << node->debug_id() << "\" [style=\"filled\", fillcolor=\"#FFFFFF\"]" << std::endl;
	}
	for (Link link : this->links.links) {
		ss << "    \"" << link.from.node()->debug_id() << "\" -> \"" << link.to.node()->debug_id() << "\"" << std::endl;
	}

	for (Node *node : marked_nodes) {
		ss << "    \"" << node->debug_id() << "\" [fillcolor=\"#FFAAAA\"]" << std::endl;
	}

	ss << "}" << std::endl;
	return ss.str();
}

} /* namespace LLVMNodeCompiler */