import bpy
from .. base import FunctionNode

class CombineListsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineListsNode"
    bl_label = "Combine Lists"

    def get_sockets(self):
        return [
            ("fn_FloatListSocket", "List 1"),
            ("fn_FloatListSocket", "List 2"),
        ], [
            ("fn_FloatListSocket", "List"),
        ]