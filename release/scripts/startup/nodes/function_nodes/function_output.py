import bpy
from .. base import BaseNode
from .. node_builder import NodeBuilder

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    variadic: NodeBuilder.VariadicProperty()

    def declaration(self, builder):
        builder.variadic_input("inputs", "variadic", "New Output")
