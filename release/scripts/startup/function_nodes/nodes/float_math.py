import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

operation_items = [
    ("ADD", "Add", "", "", 1),
    ("MULTIPLY", "Multiply", "", "", 2),
    ("MIN", "Minimum", "", "", 3),
    ("MAX", "Maximum", "", "", 4),
    ("SIN", "Sin", "", "", 5),
]

single_value_operations = {
    "SIN",
}

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
        update=FunctionNode.refresh,
    )

    use_list__a: SocketBuilder.VectorizedProperty()
    use_list__b: SocketBuilder.VectorizedProperty()

    def declaration(self, builder: SocketBuilder):
        builder.vectorized_input(
            "a", "use_list__a",
            "A", "A", "Float")
        builder.vectorized_input(
            "b", "use_list__b",
            "B", "B", "Float")

        builder.vectorized_output(
            "result", ["use_list__a", "use_list__b"],
            "Result", "Result", "Float")

    def draw(self, layout):
        layout.prop(self, "operation", text="")