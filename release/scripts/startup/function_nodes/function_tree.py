import bpy
from collections import namedtuple

from . base import BaseTree

FunctionInput = namedtuple("FunctionInput",
    ["data_type", "name", "identifier"])

FunctionOutput = namedtuple("FunctionOutput",
    ["data_type", "name", "identifier"])

class FunctionTree(bpy.types.NodeTree, BaseTree):
    bl_idname = "FunctionTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"

    def interface_changed(self):
        for tree in bpy.data.node_groups:
            if not isinstance(tree, BaseTree):
                continue
            for node in tree.nodes:
                if node.bl_idname == "fn_CallNode":
                    node.rebuild_and_try_keep_state()

    def iter_function_inputs(self):
        node = self.get_input_node()
        if node is None:
            return

        for socket in node.outputs[:-1]:
            yield FunctionInput(
                socket.data_type,
                "hello",
                socket.identifier)

    def iter_function_outputs(self):
        node = self.get_output_node()
        if node is None:
            return

        for socket in node.inputs[:-1]:
            yield FunctionOutput(
                socket.data_type,
                "bye",
                socket.identifier)

    def get_input_node(self):
        for node in self.nodes:
            if node.bl_idname == "fn_FunctionInputNode":
                return node
        return None

    def get_output_node(self):
        for node in self.nodes:
            if node.bl_idname == "fn_FunctionOutputNode":
                return node
        return None