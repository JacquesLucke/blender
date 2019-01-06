#include "node_compiler.hpp"

#include <sstream>
#include <iostream>

namespace LLVMNodeCompiler {

/* Type
 ****************************************/

llvm::Type *Type::getLLVMType(llvm::LLVMContext &context)
{
	if (!this->typePerContext.contains(&context)) {
		llvm::Type *type = this->createLLVMType(context);
		this->typePerContext.add(&context, type);
	}
	return this->typePerContext.lookup(&context);
}

llvm::Value *Type::buildCopyIR(
	llvm::IRBuilder<> &UNUSED(builder),
	llvm::Value *value)
{
	return value;
}

void Type::buildFreeIR(
	llvm::IRBuilder<> &UNUSED(builder),
	llvm::Value *UNUSED(value))
{
	return;
}


/* LinkSet
 ********************************************/

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

SocketSet LinkSet::getTargetSockets(AnySocket socket) const
{
	assert(socket.is_output());

	SocketSet targets;
	for (Link link : this->links) {
		if (link.from == socket) {
			targets.add(link.to);
		}
	}

	return targets;
}

SocketSet DataFlowGraph::getTargetSockets(AnySocket socket) const
{
	return this->links.getTargetSockets(socket);
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

Type *AnySocket::type() const
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
	ss << "Node at " << (void *)this;
	return ss.str();
}

/* IR Utils
 ********************************************/

llvm::CallInst *callPointer(
	llvm::IRBuilder<> &builder,
	void *pointer, llvm::FunctionType *type, llvm::ArrayRef<llvm::Value *> arguments)
{
	auto address_int = builder.getInt64((size_t)pointer);
	auto address = builder.CreateIntToPtr(address_int, type->getPointerTo());
	return builder.CreateCall(address, arguments);
}

llvm::Value *voidPtrToIR(llvm::IRBuilder<> &builder, void *pointer)
{
	return ptrToIR(builder, pointer, getVoidPtrTy(builder));
}

llvm::Value *ptrToIR(llvm::IRBuilder<> &builder, void *pointer, llvm::Type *type)
{
	return builder.CreateIntToPtr(builder.getInt64((size_t)pointer), type);
}

llvm::Type *getVoidPtrTy(llvm::IRBuilder<> &builder)
{
	return builder.getVoidTy()->getPointerTo();
}

llvm::Type *getVoidPtrTy(llvm::LLVMContext &context)
{
	return llvm::Type::getVoidTy(context)->getPointerTo();
}


/* DataFlowGraph
 **********************************************/

void DataFlowCallable::printCode()
{
	this->module->print(llvm::outs(), nullptr);
}


DataFlowCallable *DataFlowGraph::generateCallable(
	std::string debug_name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	llvm::LLVMContext *context = new llvm::LLVMContext();

	std::string module_name = debug_name + " Module";
	std::string function_name = debug_name + " Function";

	llvm::Module *module = this->generateModule(
		*context, module_name, function_name, inputs, outputs);

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
	llvm::LLVMContext &context,
	std::string module_name, std::string function_name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	assert(outputs.size() > 0);
	llvm::Module *module = new llvm::Module(module_name, context);
	this->generateFunction(module, function_name, inputs, outputs);
	module->print(llvm::outs(), nullptr);
	return module;
}

llvm::Function *DataFlowGraph::generateFunction(
	llvm::Module *module, std::string name,
	SocketArraySet &inputs, SocketArraySet &outputs)
{
	llvm::LLVMContext &context = module->getContext();

	std::vector<llvm::Type *> input_types;
	for (AnySocket socket : inputs) {
		input_types.push_back(socket.type()->getLLVMType(context));
	}

	std::vector<llvm::Type *> output_types;
	for (AnySocket socket : outputs) {
		output_types.push_back(socket.type()->getLLVMType(context));
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
	this->generateCode(builder, inputs, outputs, input_values, output_values);

	llvm::Value *output = llvm::UndefValue::get(return_type);
	for (uint i = 0; i < outputs.size(); i++) {
		output = builder.CreateInsertValue(output, output_values[i], i);
	}
	builder.CreateRet(output);

	llvm::verifyFunction(*function, &llvm::outs());
	llvm::verifyModule(*module, &llvm::outs());

	return function;
}

void DataFlowGraph::generateCode(
	llvm::IRBuilder<> &builder,
	SocketArraySet &inputs, SocketArraySet &outputs, std::vector<llvm::Value *> &input_values,
	std::vector<llvm::Value *> &r_output_values)
{
	assert(outputs.size() > 0);
	assert(inputs.size() == input_values.size());

	SocketSet required_sockets = this->findRequiredSockets(inputs, outputs);

	SocketValueMap values;
	for (uint i = 0; i < inputs.size(); i++) {
		values.add(inputs[i], input_values[i]);
	}

	SocketSet forwarded_sockets;
	for (AnySocket socket : outputs) {
		this->generateCodeForSocket(builder, socket, values, required_sockets, forwarded_sockets);
		r_output_values.push_back(values.lookup(socket));
	}
}

void DataFlowGraph::generateCodeForSocket(
	llvm::IRBuilder<> &builder,
	AnySocket socket,
	SocketValueMap &values,
	SocketSet &required_sockets,
	SocketSet &forwarded_sockets)
{
	if (values.contains(socket)) {
		/* do nothing */
	}
	else if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		this->generateCodeForSocket(builder, origin, values, required_sockets, forwarded_sockets);
		if (!forwarded_sockets.contains(origin)) {
			this->forwardOutputToRequiredInputs(builder, origin, values, required_sockets);
			forwarded_sockets.add(origin);
		}
	}
	else if (socket.is_output()) {
		Node *node = socket.node();
		std::vector<llvm::Value *> input_values;
		for (uint i = 0; i < node->inputs().size(); i++) {
			AnySocket input = node->Input(i);
			this->generateCodeForSocket(builder, input, values, required_sockets, forwarded_sockets);
			input_values.push_back(values.lookup(input));
		}

		std::vector<llvm::Value *> output_values;
		node->buildIR(builder, input_values, output_values);

		for (uint i = 0; i < node->outputs().size(); i++) {
			values.add(node->Output(i), output_values[i]);
		}
	}
	else {
		assert(!"should never happen");
	}

}

void DataFlowGraph::forwardOutputToRequiredInputs(
	llvm::IRBuilder<> &builder,
	AnySocket output,
	SocketValueMap &values,
	SocketSet &required_sockets)
{
	llvm::Value *value_to_forward = values.lookup(output);
	Type *type = output.type();

	SocketArraySet targets;
	for (AnySocket target : this->getTargetSockets(output)) {
		if (required_sockets.contains(target) && !values.contains(target)) {
			assert(type == target.type());
			targets.add(target);
		}
	}

	if (targets.size() == 0) {
		type->buildFreeIR(builder, value_to_forward);
	}
	else if (targets.size() == 1) {
		values.add(targets[0], value_to_forward);
	}
	else {
		values.add(targets[0], value_to_forward);
		for (uint i = 1; i < targets.size(); i++) {
			AnySocket target = targets[i];
			llvm::Value *copied_value = type->buildCopyIR(builder, value_to_forward);
			values.add(target, copied_value);
		}
	}
}

SocketSet DataFlowGraph::findRequiredSockets(SocketSet &inputs, SocketSet &outputs)
{
	SocketSet required_sockets;

	for (AnySocket socket : outputs) {
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