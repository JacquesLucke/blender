import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

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
        update=FunctionNode.sync_tree,
    )

    use_list__a: NodeBuilder.VectorizedProperty()
    use_list__b: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "a", "use_list__a",
            "A", "A", "Float")
        prop_names = ["use_list__a"]

        if self.operation not in single_value_operations:
            builder.vectorized_input(
                "b", "use_list__b",
                "B", "B", "Float")
            prop_names.append("use_list__b")


        builder.vectorized_output(
            "result", prop_names,
            "Result", "Result", "Float")

    def draw(self, layout):
        layout.prop(self, "operation", text="")
