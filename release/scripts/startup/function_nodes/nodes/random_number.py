import bpy
from .. base import FunctionNode

class RandomNumberNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_RandomNumberNode"
    bl_label = "Random Number"

    def get_sockets(self):
        return [
            ("fn_IntegerSocket", "Seed"),
            ("fn_FloatSocket", "Min"),
            ("fn_FloatSocket", "Max"),
        ], [
            ("fn_FloatSocket", "Value"),
        ]