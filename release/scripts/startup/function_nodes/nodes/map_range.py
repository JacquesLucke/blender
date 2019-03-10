import bpy
from bpy.props import *
from .. base import FunctionNode

class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    def get_sockets(self):
        return [
            ("fn_FloatSocket", "Value"),
            ("fn_FloatSocket", "From Min"),
            ("fn_FloatSocket", "From Max"),
            ("fn_FloatSocket", "To Min"),
            ("fn_FloatSocket", "To Max"),
        ], [
            ("fn_FloatSocket", "Value"),
        ]