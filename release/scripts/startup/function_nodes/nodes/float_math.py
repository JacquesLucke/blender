import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

operation_items = [
    ("ADD", "Add", "", "", 1),
    ("MULTIPLY", "Multiply", "", "", 2),
    ("MIN", "Minimum", "", "", 3),
    ("MAX", "Maximum", "", "", 4),
]

class FloatMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatMathNode"
    bl_label = "Float Math"

    operation: EnumProperty(
        name="Operation",
        items=operation_items,
    )

    def get_sockets(self):
        return [
            FixedSocketDecl("a", "A", "Float"),
            FixedSocketDecl("b", "B", "Float"),
        ], [
            FixedSocketDecl("result", "Result", "Float"),
        ]

    def draw(self, layout):
        layout.prop(self, "operation", text="")