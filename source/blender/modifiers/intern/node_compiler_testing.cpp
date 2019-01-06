#include <iostream>
#include "LLVMNodeCompiler/node_compiler.hpp"

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
}

namespace NC = LLVMNodeCompiler;

class IntegerType : public NC::Type {
public:
	IntegerType(uint bits)
		: bits(bits) {}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return llvm::Type::getIntNTy(context, this->bits);
	}

private:
	uint bits;
};

struct MyTypeStruct {
	int a, b, c;
};

class MyType : public NC::PointerType<MyTypeStruct> {
public:
	MyTypeStruct *copy(MyTypeStruct *value)
	{
		return new MyTypeStruct(*value);
	}

	void free(MyTypeStruct *value)
	{
		delete value;
	}
};

auto *type_int32 = new IntegerType(32);
auto *type_custom = new MyType();


class MyTypeInputNode : public NC::ExecuteFunctionNode {
public:
	MyTypeInputNode(int a, int b, int c)
	{
		this->data.a = a;
		this->data.b = b;
		this->data.c = c;
		this->addOutput("Value", type_custom);
		this->execute_function = (void *)execute;
		this->use_this = true;
	}

	std::string debug_name() const { return "Type Input"; }

private:
	static void execute(MyTypeInputNode *node, void **r_value)
	{
		*r_value = new MyTypeStruct(node->data);
	}

	MyTypeStruct data;
};

class MyTypePrintNode : public NC::ExecuteFunctionNode {
public:
	MyTypePrintNode()
	{
		this->addInput("A", type_custom);
		this->addInput("B", type_custom);
		this->addOutput("Output", type_int32);
		this->addOutput("lala", type_custom);
		this->execute_function = (void *)execute;
	}

	std::string debug_name() const { return "Print Type"; }

private:
	static void execute(MyTypeStruct *a, MyTypeStruct *b, int *r_value, MyTypeStruct **r_lala)
	{
		std::cout << "A: " << a->a << " " << a->b << " " << a->c << std::endl;
		std::cout << "B: " << b->a << " " << b->b << " " << b->c << std::endl;
		*r_value = 1111;
		printf("%p\n%p\n", a, b);
		delete a;
		*r_lala = b;
	}
};

class ModifyMyTypeNode : public NC::ExecuteFunctionNode {
public:
	ModifyMyTypeNode()
	{
		this->addInput("In", type_custom);
		this->addOutput("Out", type_custom);
		this->execute_function = (void *)execute;
	}

	std::string debug_name() const { return "Modify Type"; }

private:
	static void execute(MyTypeStruct *data, MyTypeStruct **r_data)
	{
		data->a = 200;
		*r_data = data;
	}
};


class IntInputNode : public NC::Node {
public:
	IntInputNode(int number)
		: number(number)
	{
		this->addOutput("Value", type_int32);
	}

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder.getInt32(this->number));
	}

	std::string debug_name() const
	{ return "Int Input " + std::to_string(this->number); }

private:
	int number;
};

class IntRefInputNode : public NC::Node {
public:
	IntRefInputNode(int *pointer)
		: pointer(pointer)
	{
		this->addOutput("Value", type_int32);
	}

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs)
	{
		auto address = NC::ptrToIR(builder, this->pointer, builder.getInt32Ty());
		r_outputs.push_back(builder.CreateLoad(address));
	}

private:
	int *pointer;
};

class AddIntegersNode : public NC::Node {
public:
	AddIntegersNode()
	{
		this->addInput("A", type_int32);
		this->addInput("B", type_int32);
		this->addOutput("Result", type_int32);
	}

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs)
	{
		r_outputs.push_back(builder.CreateAdd(inputs[0], inputs[1]));
	}
};

class PrintIntegerNode : public NC::ExecuteFunctionNode {
public:
	PrintIntegerNode()
	{
		this->addInput("In", type_int32);
		this->addOutput("Out", type_int32);
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

	std::string prefix;
};

class SwitchIntegerNode : public NC::Node {
public:
	SwitchIntegerNode(uint amount)
	{
		this->amount = amount;

		this->addInput("Switch", type_int32);
		for (int i = 0; i < amount; i++) {
			this->addInput("Input (" + std::to_string(i) + ")", type_int32);
		}
		this->addOutput("Selected", type_int32);
	}

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs)
	{
		llvm::LLVMContext &context = builder.getContext();
		llvm::Function *function = builder.GetInsertBlock()->getParent();

		llvm::BasicBlock *final_block = llvm::BasicBlock::Create(context, "After Switch", function);
		llvm::IRBuilder<> final_builder(final_block);
		llvm::PHINode *phi = final_builder.CreatePHI(type_int32->getLLVMType(context), amount);

		llvm::BasicBlock *default_dest = llvm::BasicBlock::Create(context, "Case Default", function);
		llvm::IRBuilder<> default_builder(default_dest);
		default_builder.CreateBr(final_block);

		phi->addIncoming(final_builder.getInt32(0), default_dest);

		llvm::SwitchInst *switch_inst = builder.CreateSwitch(inputs[0], default_dest, this->amount);
		for (uint i = 0; i < this->amount; i++) {
			auto case_block = llvm::BasicBlock::Create(builder.getContext(), "Case " + std::to_string(i), function);
			switch_inst->addCase(builder.getInt32(i), case_block);

			llvm::IRBuilder<> case_builder(case_block);
			case_builder.CreateBr(final_block);

			phi->addIncoming(inputs[i + 1], case_block);
		}

		r_outputs.push_back(phi);
		builder.SetInsertPoint(final_block);
	}

	std::string debug_name() const { return "Int Switch"; }

private:
	uint amount;
};


extern "C" {
	void run_tests(void);
}

void run_tests()
{
	auto caseIn = new IntInputNode(0);
	auto in1 = new IntInputNode(10);
	auto in2 = new IntInputNode(200);
	auto in3 = new IntInputNode(3000);
	auto selector1 = new SwitchIntegerNode(3);

	NC::DataFlowGraph graph;
	graph.nodes.push_back(caseIn);
	graph.nodes.push_back(in1);
	graph.nodes.push_back(in2);
	graph.nodes.push_back(in3);
	graph.nodes.push_back(selector1);

	graph.links.links.push_back(NC::Link(caseIn->Output(0), selector1->Input(0)));
	graph.links.links.push_back(NC::Link(in1->Output(0), selector1->Input(1)));
	graph.links.links.push_back(NC::Link(in2->Output(0), selector1->Input(2)));
	graph.links.links.push_back(NC::Link(in3->Output(0), selector1->Input(3)));

	NC::SocketArraySet inputs = { selector1->Input(0) };
	NC::SocketArraySet outputs = { selector1->Output(0) };
	NC::DataFlowCallable *callable = graph.generateCallable("Hello", inputs, outputs);

	//callable->printCode();
	int result = ((int (*)(int))callable->getFunctionPointer())(156);
	std::cout << result << std::endl;

	auto dot = graph.toDotFormat();
	//std::cout << dot << std::endl;
	WM_clipboard_text_set(dot.c_str(), false);

	std::cout << "Test Finished" << std::endl;
}