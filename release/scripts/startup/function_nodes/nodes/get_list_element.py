import bpy
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: SocketBuilder.ListTypeProperty()

    def declaration(self, builder: SocketBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_input("index", "Index", "Integer")
        builder.dynamic_base_input("fallback", "Fallback", "active_type")
        builder.dynamic_base_output("value", "Value", "active_type")