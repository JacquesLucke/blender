import bpy
from .. base import FunctionNode

class VectorDistanceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorDistanceNode"
    bl_label = "Vector Distance"

    def get_sockets(self):
        return [
            ("fn_VectorSocket", "A"),
            ("fn_VectorSocket", "B"),
        ], [
            ("fn_FloatSocket", "Distance"),
        ]
