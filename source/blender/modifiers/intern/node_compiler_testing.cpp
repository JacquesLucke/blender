#include "node_compiler.hpp"
#include "BLI_utildefines.h"

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
}

#include <iostream>

namespace NC = LLVMNodeCompiler;

class IntegerType : public NC::Type {
private:
	uint bits;

public:
	IntegerType(uint bits)
		: bits(bits) {}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return llvm::Type::getIntNTy(context, this->bits);
	}
};

auto *type_int32 = new IntegerType(32);

class IntInputNode : public NC::SingleBuilderNode {
private:
	int number;

public:
	IntInputNode(int number)
		: number(number)
	{
		this->m_outputs.add("Value", type_int32);
	}

	void buildLLVMIR(
		llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder->getInt32(this->number));
	}
};

class IntRefInputNode : public NC::SingleBuilderNode {
private:
	int *pointer;

public:
	IntRefInputNode(int *pointer)
		: pointer(pointer)
	{
		this->m_outputs.add("Value", type_int32);
	}

	void buildLLVMIR(
		llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs)
	{
		auto address_int = builder->getInt64((size_t)this->pointer);
		auto address = builder->CreateIntToPtr(address_int, llvm::Type::getInt32PtrTy(builder->getContext()));
		r_outputs.push_back(builder->CreateLoad(address));
	}
};

class AddIntegersNode : public NC::SingleBuilderNode {
public:
	AddIntegersNode()
	{
		this->m_inputs.add("A", type_int32);
		this->m_inputs.add("B", type_int32);
		this->m_outputs.add("Result", type_int32);
	}

	void buildLLVMIR(
		llvm::IRBuilder<> *builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder->CreateAdd(inputs[0], inputs[1]));
	}
};


extern "C" {
	void run_tests(void);
}

void run_tests()
{
	int test_value = 1000;

	auto in1 = new IntInputNode(1);
	auto in2 = new IntRefInputNode(&test_value);
	auto in3 = new IntInputNode(10);

	auto add1 = new AddIntegersNode();
	auto add2 = new AddIntegersNode();
	auto add3 = new AddIntegersNode();

	NC::DataFlowGraph graph;
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


	NC::SocketArraySet inputs = { in1->Output(0), in2->Output(0) };
	NC::SocketArraySet outputs = { add3->Output(0) };
	NC::DataFlowCallable *callable = graph.generateCallable("Hello", inputs, outputs);

	callable->printCode();
	int result = ((int (*)(int, int))callable->getFunctionPointer())(10, 25);
	std::cout << result << std::endl;

	// NC::SocketSet inputs = { add1->Input(0), add1->Input(1), add2->Input(1) };
	// NC::SocketSet outputs = { add3->Output(0) };

	// auto required_sockets = graph.findRequiredSockets(inputs, outputs);

	// std::vector<NC::Node *> required_nodes;
	// for (NC::AnySocket socket : required_sockets.elements()) {
	// 	required_nodes.push_back(socket.node());
	// }

	// auto dot = graph.toDotFormat(required_nodes);
	// std::cout << dot << std::endl;
	// WM_clipboard_text_set(dot.c_str(), false);

	std::cout << "Test Finished" << std::endl;
}