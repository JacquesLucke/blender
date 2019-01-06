#include <iostream>
#include "node_compiler.hpp"

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
}

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

struct MyTypeStruct {
	int a, b, c;
};

class MyType : public NC::Type {
public:
	MyType() {}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return llvm::Type::getVoidTy(context)->getPointerTo();
	}

	llvm::Value *buildCopyIR(llvm::IRBuilder<> &builder, llvm::Value *value)
	{
		llvm::LLVMContext &context = builder.getContext();
		llvm::FunctionType *ftype = llvm::FunctionType::get(
			this->getLLVMType(context), this->getLLVMType(context), false);
		return NC::callPointer(builder, (void *)copy, ftype, value);
	}

	static MyTypeStruct *copy(MyTypeStruct *value)
	{
		return new MyTypeStruct(*value);
	}
};

auto *type_int32 = new IntegerType(32);
auto *type_custom = new MyType();


class MyTypeInputNode : public NC::ExecuteFunctionNode {
private:
	MyTypeStruct data;
public:
	MyTypeInputNode(int a, int b, int c)
	{
		this->data.a = a;
		this->data.b = b;
		this->data.c = c;
		this->m_outputs.add("Value", type_custom);
		this->execute_function = (void *)my_type_input;
		this->use_this = true;
	}

private:
	static void my_type_input(MyTypeInputNode *node, void **r_value)
	{
		*r_value = new MyTypeStruct(node->data);
	}
};

class MyTypePrintNode : public NC::ExecuteFunctionNode {
public:
	MyTypePrintNode()
	{
		this->m_inputs.add("A", type_custom);
		this->m_inputs.add("B", type_custom);
		this->m_outputs.add("Output", type_int32);
		this->execute_function = (void *)my_type_print;
	}

private:
	static void my_type_print(MyTypeStruct *a, MyTypeStruct *b, int *r_value)
	{
		std::cout << "A: " << a->a << " " << a->b << " " << a->c << std::endl;
		std::cout << "B: " << b->a << " " << b->b << " " << b->c << std::endl;
		*r_value = 568;
		printf("%p\n%p\n", a, b);
		delete a;
		delete b;
	}
};

class ModifyMyTypeNode : public NC::ExecuteFunctionNode {
public:
	ModifyMyTypeNode()
	{
		this->m_inputs.add("In", type_custom);
		this->m_outputs.add("Out", type_custom);
		this->execute_function = (void *)modify_my_type;
	}

private:
	static void modify_my_type(MyTypeStruct *data, MyTypeStruct **r_data)
	{
		data->a = 200;
		*r_data = data;
	}
};


class IntInputNode : public NC::Node {
private:
	int number;

public:
	IntInputNode(int number)
		: number(number)
	{
		this->m_outputs.add("Value", type_int32);
	}

	void buildLLVMIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder.getInt32(this->number));
	}
};

class IntRefInputNode : public NC::Node {
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

class AddIntegersNode : public NC::Node {
public:
	AddIntegersNode()
	{
		this->m_inputs.add("A", type_int32);
		this->m_inputs.add("B", type_int32);
		this->m_outputs.add("Result", type_int32);
	}

	void buildLLVMIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder.CreateAdd(inputs[0], inputs[1]));
	}
};

class PrintIntegerNode : public NC::ExecuteFunctionNode {
private:
	std::string prefix;

public:
	PrintIntegerNode()
	{
		this->m_inputs.add("In", type_int32);
		this->m_outputs.add("Out", type_int32);
		this->execute_function = (void *)print_integer;
		this->use_this = true;
		this->prefix = "Hello Number ";
	}

private:
	static void print_integer(PrintIntegerNode *node, int number, int *r_number)
	{
		std::cout << node->prefix << number << std::endl;
		*r_number = number + 42;
	}
};


extern "C" {
	void run_tests(void);
}

void run_tests()
{
	auto in1 = new MyTypeInputNode(10, 20, 30);
	auto mod1 = new ModifyMyTypeNode();
	auto print1 = new MyTypePrintNode();

	NC::DataFlowGraph graph;
	graph.nodes.push_back(in1);
	graph.nodes.push_back(print1);
	graph.links.links.push_back(NC::Link(in1->Output(0), print1->Input(0)));
	graph.links.links.push_back(NC::Link(in1->Output(0), mod1->Input(0)));
	graph.links.links.push_back(NC::Link(mod1->Output(0), print1->Input(1)));

	NC::SocketArraySet inputs = { };
	NC::SocketArraySet outputs = { print1->Output(0) };
	NC::DataFlowCallable *callable = graph.generateCallable("Hello", inputs, outputs);

	//callable->printCode();
	int result = ((int (*)())callable->getFunctionPointer())();
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