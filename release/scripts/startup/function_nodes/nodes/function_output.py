import bpy
from .. base import BaseNode
from .. socket_builder import SocketBuilder

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    variadic: SocketBuilder.VariadicProperty()

    def declaration(self, builder):
        builder.variadic_input("inputs", "variadic", "New Output")

    def on_rebuild_post(self):
        self.tree.interface_changed()