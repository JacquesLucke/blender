import bpy
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class SeparateColorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateColorNode"
    bl_label = "Separate Color"

    use_list__color: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "color", "use_list__color",
            "Color", "Colors", "Color")

        builder.vectorized_output(
            "red", ["use_list__color"],
            "Red", "Red", "Float")
        builder.vectorized_output(
            "green", ["use_list__color"],
            "Green", "Green", "Float")
        builder.vectorized_output(
            "blue", ["use_list__color"],
            "Blue", "Blue", "Float")
        builder.vectorized_output(
            "alpha", ["use_list__color"],
            "Alpha", "Alpha", "Float")
