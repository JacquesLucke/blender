import bpy
from .. base import FunctionNode

class CombineVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineVectorNode"
    bl_label = "Combine Vector"

    def get_sockets(self):
        return [
            ("fn_FloatSocket", "X"),
            ("fn_FloatSocket", "Y"),
            ("fn_FloatSocket", "Z"),
        ], [
            ("fn_VectorSocket", "Result"),
        ]

bpy.utils.register_class(CombineVectorNode)