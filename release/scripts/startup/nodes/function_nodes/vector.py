import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder


class VectorMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorMathNode"
    bl_label = "Vector Math"

    operation_items = [
        ("ADD", "Add", "", "", 1),
        ("SUB", "Subtract", "", "", 2),
        ("MUL", "Multiply", "", "", 3),
        ("DIV", "Divide", "", "", 4),
        ("CROSS", "Cross Product", "", "", 5),
        ("REFLECT", "Reflect", "", "", 6),
        ("PROJECT", "Project", "", "", 7),
        ("DOT", "Dot", "", "", 8),
    ]

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
            "A", "A", "Vector")
        builder.vectorized_input(
            "b", "use_list__b",
            "B", "B", "Vector")

        if self.operation == "DOT":
            builder.vectorized_output(
                "result", ["use_list__a", "use_list__b"],
                "Result", "Result", "Float")
        else:
            builder.vectorized_output(
                "result", ["use_list__a", "use_list__b"],
                "Result", "Result", "Vector")

    def draw(self, layout):
        layout.prop(self, "operation", text="")


class VectorDistanceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorDistanceNode"
    bl_label = "Vector Distance"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("a", "A", "Vector")
        builder.fixed_input("b", "B", "Vector")
        builder.fixed_output("distance", "Distance", "Float")


class CombineVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineVectorNode"
    bl_label = "Combine Vector"

    use_list__x: NodeBuilder.VectorizedProperty()
    use_list__y: NodeBuilder.VectorizedProperty()
    use_list__z: NodeBuilder.VectorizedProperty()

    def declaration(self, builder):
        builder.vectorized_input(
            "x", "use_list__x",
            "X", "X", "Float")
        builder.vectorized_input(
            "y", "use_list__y",
            "Y", "Y", "Float")
        builder.vectorized_input(
            "z", "use_list__z",
            "Z", "Z", "Float")

        builder.vectorized_output(
            "vector", ["use_list__x", "use_list__y", "use_list__z"],
            "Vector", "Vectors", "Vector")


class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    use_list__vector: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "vector", "use_list__vector",
            "Vector", "Vectors", "Vector")

        builder.vectorized_output(
            "x", ["use_list__vector"],
            "X", "X", "Float")
        builder.vectorized_output(
            "y", ["use_list__vector"],
            "Y", "Y", "Float")
        builder.vectorized_output(
            "z", ["use_list__vector"],
            "Z", "Z", "Float")
