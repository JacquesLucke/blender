import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

operation_items = [
    ("AND", "And", "", "", 1),
    ("OR", "Or", "", "", 2),
    ("NOT", "Not", "", "", 3),
]
single_value_operations = {
    "NOT"
}
class BooleanMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_BooleanMathNode"
    bl_label = "Boolean Math"

    search_terms = (
        ("And", {"operation" : "AND"}),
        ("Or", {"operation" : "OR"}),
        ("Not", {"operation" : "NOT"}),
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
            "A", "A", "Boolean")
        prop_names = ["use_list__a"]

        if self.operation not in single_value_operations:
            builder.vectorized_input(
                "b", "use_list__b",
                "B", "B", "Boolean")
            prop_names.append("use_list__b")


        builder.vectorized_output(
            "result", prop_names,
            "Result", "Result", "Boolean")

    def draw(self, layout):
        layout.prop(self, "operation", text="")
