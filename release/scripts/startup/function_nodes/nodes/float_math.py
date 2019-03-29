import bpy
from bpy.props import *
from .. base import FunctionNode

operation_items = [
    ("ADD", "Add", "", "", 1),
    ("MULTIPLY", "Multiply", "", "", 2),
    ("MIN", "Minimum", "", "", 3),
    ("MAX", "Maximum", "", "", 4),
]

class FloatMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatMathNode"
    bl_label = "Float Math"

    search_terms = (
        ("Add Floats", {"operation" : "ADD"}),
        ("Multiply Floats", {"operation" : "MULTIPLY"}),
    )

    operation: EnumProperty(
        name="Operation",
        items=operation_items,
    )

    def declaration(self, builder):
        builder.fixed_input("a", "A", "Float")
        builder.fixed_input("b", "B", "Float")
        builder.fixed_output("result", "Result", "Float")

    def draw(self, layout):
        layout.prop(self, "operation", text="")