import bpy
from .. base import BaseNode
from .. node_builder import NodeBuilder

class FunctionInputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    variadic: NodeBuilder.VariadicProperty()

    def declaration(self, builder):
        builder.variadic_output("outputs", "variadic", "New Input")
