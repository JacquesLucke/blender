import bpy
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: NodeBuilder.DynamicListProperty()

    def declaration(self, builder: NodeBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_input("index", "Index", "Integer")
        builder.dynamic_base_input("fallback", "Fallback", "active_type")
        builder.dynamic_base_output("value", "Value", "active_type")
