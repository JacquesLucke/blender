import bpy
from .. base import FunctionNode

class AddFloatsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_AddFloatsNode"
    bl_label = "Add Floats"

    def get_sockets(self):
        return [
            ("fn_FloatSocket", "A"),
            ("fn_FloatSocket", "B"),
        ], [
            ("fn_FloatSocket", "Result"),
        ]

bpy.utils.register_class(AddFloatsNode)