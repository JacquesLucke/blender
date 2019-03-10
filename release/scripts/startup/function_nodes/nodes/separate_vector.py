import bpy
from .. base import FunctionNode

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    def get_sockets(self):
        return [
            ("fn_VectorSocket", "Vector"),
        ], [
            ("fn_FloatSocket", "X"),
            ("fn_FloatSocket", "Y"),
            ("fn_FloatSocket", "Z"),
        ]
