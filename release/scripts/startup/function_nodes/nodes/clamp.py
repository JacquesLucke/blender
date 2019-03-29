import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class ClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClampNode"
    bl_label = "Clamp"

    def get_sockets(self):
        return [
            FixedSocketDecl(self, "value", "Value", "Float"),
            FixedSocketDecl(self, "min", "Min", "Float"),
            FixedSocketDecl(self, "max", "Max", "Float"),
        ], [
            FixedSocketDecl(self, "result", "Result", "Float"),
        ]