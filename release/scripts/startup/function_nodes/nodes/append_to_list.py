import bpy
from .. base import FunctionNode

class AppendToListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_AppendToListNode"
    bl_label = "Append to List"

    def get_sockets(self):
        return [
            ("fn_FloatListSocket", "List"),
            ("fn_FloatSocket", "Value"),
        ], [
            ("fn_FloatListSocket", "List"),
        ]