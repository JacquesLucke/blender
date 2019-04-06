import bpy
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    use_list__vector: SocketBuilder.VectorizedInputProperty()
    use_list__x: SocketBuilder.VectorizedOutputProperty()
    use_list__y: SocketBuilder.VectorizedOutputProperty()
    use_list__z: SocketBuilder.VectorizedOutputProperty()

    def declaration(self, builder: SocketBuilder):
        builder.vectorized_input(
            "vector", "use_list__vector",
            "Vector", "Vectors", "Vector")

        builder.vectorized_output(
            "x", "use_list__x",
            ["use_list__vector"],
            "X", "X", "Float")
        builder.vectorized_output(
            "y", "use_list__y",
            ["use_list__vector"],
            "Y", "Y", "Float")
        builder.vectorized_output(
            "z", "use_list__z",
            ["use_list__vector"],
            "Z", "Z", "Float")
