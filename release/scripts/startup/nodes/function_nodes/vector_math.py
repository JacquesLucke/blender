import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

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

    use_list__a: NodeBuilder.VectorizedProperty()
    use_list__b: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "a", "use_list__a",
            "A", "A", "Vector")
        builder.vectorized_input(
            "b", "use_list__b",
            "B", "B", "Vector")

        builder.vectorized_output(
            "result", ["use_list__a", "use_list__b"],
            "Result", "Result", "Vector")

    def draw(self, layout):
        layout.prop(self, "operation", text="")
