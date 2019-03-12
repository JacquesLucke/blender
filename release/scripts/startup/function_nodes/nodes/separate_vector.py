import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    def get_sockets(self):
        return [
            FixedSocketDecl("Vector", "Vector"),
        ], [
            FixedSocketDecl("X", "Float"),
            FixedSocketDecl("Y", "Float"),
            FixedSocketDecl("Z", "Float"),
        ]
