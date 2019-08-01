import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

operation_items = [
    ("LESS_THAN", "Less Than", "A < B", "", 1),
]

class CompareNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CompareNode"
    bl_label = "Compare"

    search_terms = (
        ("Less Than", {"operation" : "LESS_THAN"}),
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
        builder.vectorized_input(
            "b", "use_list__b",
            "B", "B", "Float")

        builder.vectorized_output(
            "result", ["use_list__a", "use_list__b"],
            "Result", "Result", "Boolean")

    def draw(self, layout):
        layout.prop(self, "operation", text="")
