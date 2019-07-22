import bpy
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class ListLengthNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ListLengthNode"
    bl_label = "List Length"

    active_type: NodeBuilder.DynamicListProperty()

    def declaration(self, builder: NodeBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_output("length", "Length", "Integer")
