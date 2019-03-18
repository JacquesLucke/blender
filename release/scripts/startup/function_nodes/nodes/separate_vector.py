import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    def get_sockets(self):
        return [
            FixedSocketDecl("vector", "Vector", "Vector"),
        ], [
            FixedSocketDecl("x", "X", "Float"),
            FixedSocketDecl("y", "Y", "Float"),
            FixedSocketDecl("z", "Z", "Float"),
        ]
