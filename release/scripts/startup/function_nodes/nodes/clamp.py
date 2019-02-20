import bpy
from .. base import FunctionNode

class ClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClampNode"
    bl_label = "Clamp"

    def get_sockets(self):
        return [
            ("fn_FloatSocket", "Value"),
            ("fn_FloatSocket", "Min"),
            ("fn_FloatSocket", "Max"),
        ], [
            ("fn_FloatSocket", "Result"),
        ]

bpy.utils.register_class(ClampNode)