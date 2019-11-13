import bpy
from collections import namedtuple

from . base import BaseTree, BaseNode

class FunctionTree(bpy.types.NodeTree, BaseTree):
    bl_idname = "FunctionTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"

    def get_input_nodes(self):
        input_nodes = [node for node in self.nodes if node.bl_idname == "fn_GroupDataInputNode"]
        sorted_input_nodes = sorted(input_nodes, key=lambda node: (node.sort_index, node.name))
        return sorted_input_nodes

    def get_output_nodes(self):
        output_nodes = [node for node in self.nodes if node.bl_idname == "fn_GroupDataOutputNode"]
        sorted_output_nodes = sorted(output_nodes, key=lambda node: (node.sort_index, node.name))
        return sorted_output_nodes

    def iter_dependency_trees(self):
        trees = set()
        for node in self.nodes:
            if isinstance(node, BaseNode):
                trees.update(node.iter_dependency_trees())
        yield from trees
