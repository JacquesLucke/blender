import bpy
from .. base import BaseNode
from .. socket_builder import SocketBuilder

class FunctionInputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    variadic: SocketBuilder.VariadicProperty()

    def declaration(self, builder):
        builder.variadic_output("outputs", "variadic", "New Input")

