import bpy
from bpy.props import *
from .. base import FunctionNode

operation_items = [
    ("ADD", "Add", "", "", 1),
]

class VectorMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorMathNode"
    bl_label = "Vector Math"

    operation: EnumProperty(
        name="Operation",
        items=operation_items,
        update=FunctionNode.refresh,
    )

    def declaration(self, builder):
        builder.fixed_input("a", "A", "Vector")
        builder.fixed_input("b", "B", "Vector")
        builder.fixed_output("result", "Result", "Vector")

    def draw(self, layout):
        layout.prop(self, "operation", text="")