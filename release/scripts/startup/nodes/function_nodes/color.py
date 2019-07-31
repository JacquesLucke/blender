import bpy
from bpy.props import *
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


class CombineColorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineColorNode"
    bl_label = "Combine Color"

    use_list__red: NodeBuilder.VectorizedProperty()
    use_list__green: NodeBuilder.VectorizedProperty()
    use_list__blue: NodeBuilder.VectorizedProperty()
    use_list__alpha: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "red", "use_list__red",
            "Red", "Red", "Float")
        builder.vectorized_input(
            "green", "use_list__green",
            "Green", "Green", "Float")
        builder.vectorized_input(
            "blue", "use_list__blue",
            "Blue", "Blue", "Float")
        builder.vectorized_input(
            "alpha", "use_list__alpha",
            "Alpha", "Alpha", "Float")

        builder.vectorized_output(
            "color", ["use_list__red", "use_list__green", "use_list__blue", "use_list__alpha"],
            "Color", "Colors", "Color")
