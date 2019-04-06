import bpy
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

class ListLengthNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ListLengthNode"
    bl_label = "List Length"

    active_type: SocketBuilder.ListTypeProperty()

    def declaration(self, builder: SocketBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_output("length", "Length", "Integer")