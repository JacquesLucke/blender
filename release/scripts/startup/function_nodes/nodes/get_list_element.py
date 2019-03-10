import bpy
from .. base import FunctionNode

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    def get_sockets(self):
        return [
            ("fn_FloatListSocket", "List"),
            ("fn_IntegerSocket", "Index"),
            ("fn_FloatSocket", "Fallback"),
        ], [
            ("fn_FloatSocket", "Value"),
        ]