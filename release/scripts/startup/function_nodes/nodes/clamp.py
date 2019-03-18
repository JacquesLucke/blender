import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class ClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClampNode"
    bl_label = "Clamp"

    def get_sockets(self):
        return [
            FixedSocketDecl("value", "Value", "Float"),
            FixedSocketDecl("min", "Min", "Float"),
            FixedSocketDecl("max", "Max", "Float"),
        ], [
            FixedSocketDecl("result", "Result", "Float"),
        ]