import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class ClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClampNode"
    bl_label = "Clamp"

    def get_sockets(self):
        return [
            FixedSocketDecl("Value", "Float"),
            FixedSocketDecl("Min", "Float"),
            FixedSocketDecl("Max", "Float"),
        ], [
            FixedSocketDecl("Result", "Float"),
        ]