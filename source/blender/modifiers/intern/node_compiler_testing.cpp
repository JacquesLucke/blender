#include "node_compiler.hpp"
#include "BLI_utildefines.h"

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
}

#include <iostream>

namespace NC = LLVMNodeCompiler;

static void generateCode_AddNode(
	std::vector<llvm::Value *> &inputs, llvm::IRBuilder<> *builder,
	std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder)
{
	llvm::Value *result = builder->CreateAdd(inputs[0], inputs[1]);
	r_outputs.push_back(result);
	*r_builder = builder;
}

static NC::SimpleNode *new_add_node(llvm::LLVMContext &context)
{
	auto node = new NC::SimpleNode();
	node->debug_name = "Add";
	node->inputs.push_back(NC::SocketInfo("A", llvm::Type::getInt32Ty(context)));
	node->inputs.push_back(NC::SocketInfo("B", llvm::Type::getInt32Ty(context)));
	node->outputs.push_back(NC::SocketInfo("Result", llvm::Type::getInt32Ty(context)));
	node->generateCode = generateCode_AddNode;
	return node;
}

static NC::SimpleNode *new_int_node(llvm::LLVMContext &context, int value)
{
	auto node = new NC::SimpleNode();
	node->debug_name = "Int";
	node->outputs.push_back(NC::SocketInfo("Value", llvm::Type::getInt32Ty(context)));
	node->generateCode = [value](
		std::vector<llvm::Value *> &UNUSED(inputs), llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder) {
			r_outputs.push_back(builder->getInt32(value));
			*r_builder = builder;
		};
	return node;
}

static NC::SimpleNode *new_int_ref_node(llvm::LLVMContext &context, int *value)
{
	auto node = new NC::SimpleNode();
	node->debug_name = "Int Ref";
	node->outputs.push_back(NC::SocketInfo("Value", llvm::Type::getInt32Ty(context)));
	node->generateCode = [value](
		std::vector<llvm::Value *> &UNUSED(inputs), llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &r_outputs, llvm::IRBuilder<> **r_builder) {
			auto address_int = builder->getInt64((uint64_t)value);
			auto address = builder->CreateIntToPtr(address_int, llvm::Type::getInt32PtrTy(builder->getContext()));
			r_outputs.push_back(builder->CreateLoad(address));
			*r_builder = builder;
		};
	return node;
}

extern "C" {
	void run_tests(void);
}

void run_tests()
{
	llvm::LLVMContext *_context = new llvm::LLVMContext();
	llvm::LLVMContext &context = *_context;

	int test_value = 1000;

	auto in1 = new_int_node(context, 1);
	auto in2 = new_int_ref_node(context, &test_value);
	auto in3 = new_int_node(context, 10);

	auto add1 = new_add_node(context);
	auto add2 = new_add_node(context);
	auto add3 = new_add_node(context);

	NC::Graph graph;
	graph.nodes.push_back(in1);
	graph.nodes.push_back(in2);
	graph.nodes.push_back(in3);
	graph.nodes.push_back(add1);
	graph.nodes.push_back(add2);
	graph.nodes.push_back(add3);

	graph.links.links.push_back(NC::Link(in1->Output(0), add1->Input(0)));
	graph.links.links.push_back(NC::Link(in2->Output(0), add1->Input(1)));
	graph.links.links.push_back(NC::Link(in2->Output(0), add2->Input(0)));
	graph.links.links.push_back(NC::Link(in3->Output(0), add2->Input(1)));
	graph.links.links.push_back(NC::Link(add1->Output(0), add3->Input(0)));
	graph.links.links.push_back(NC::Link(add2->Output(0), add3->Input(1)));

	llvm::Module *module = new llvm::Module("test", context);

	std::vector<NC::AnySocket> inputs = { };
	std::vector<NC::AnySocket> outputs = { add3->Output(0), add1->Input(0) };
	graph.generateFunction(module, "HelloWorld", inputs, outputs);

	module->print(llvm::outs(), nullptr);

	// NC::SocketSet inputs = { add1->Input(0), add1->Input(1), add2->Input(1) };
	// NC::SocketSet outputs = { add3->Output(0) };

	// auto required_sockets = graph.findRequiredSockets(inputs, outputs);

	// std::vector<NC::SimpleNode *> required_nodes;
	// for (NC::AnySocket socket : required_sockets.elements()) {
	// 	required_nodes.push_back(socket.node());
	// }

	// auto dot = graph.toDotFormat(required_nodes);
	// std::cout << dot << std::endl;
	// WM_clipboard_text_set(dot.c_str(), false);

	std::cout << "Test Finished" << std::endl;
}