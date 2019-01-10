#include <iostream>
#include "function_nodes/nodes/nodes.hpp"
#include "function_nodes/types/types.hpp"

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
	void run_tests(void);
}

void run_tests()
{
	auto in1 = new Int32InputNode(42);
	auto in2 = new Int32InputNode(30);
	auto add1 = new AddIntegersNode(2, type_int32);

	NC::DataFlowGraph graph;
	graph.addNode(in1);
	graph.addNode(in2);
	graph.addNode(add1);

	graph.addLink(in1->Output(0), add1->Input(0));
	graph.addLink(in2->Output(0), add1->Input(1));

	if (!graph.verify()) {
		return;
	}

	NC::SocketArraySet inputs = { };
	NC::SocketArraySet outputs = { add1->Output(0) };
	auto function = NC::compileDataFlow(graph, inputs, outputs);

	//callable->printCode();
	int result = ((int (*)())function->pointer())();
	std::cout << "Result " << result << std::endl;

	auto dot = graph.toDotFormat();
	//std::cout << dot << std::endl;
	WM_clipboard_text_set(dot.c_str(), false);

	std::cout << "Test Finished" << std::endl;
}