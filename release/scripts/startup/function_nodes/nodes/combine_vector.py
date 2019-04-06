import bpy
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

class CombineVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineVectorNode"
    bl_label = "Combine Vector"

    use_list__x: SocketBuilder.VectorizedInputProperty()
    use_list__y: SocketBuilder.VectorizedInputProperty()
    use_list__z: SocketBuilder.VectorizedInputProperty()

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
