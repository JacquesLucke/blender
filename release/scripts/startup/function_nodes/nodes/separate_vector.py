import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    def get_sockets(self):
        return [
            FixedSocketDecl(self, "vector", "Vector", "Vector"),
        ], [
            FixedSocketDecl(self, "x", "X", "Float"),
            FixedSocketDecl(self, "y", "Y", "Float"),
            FixedSocketDecl(self, "z", "Z", "Float"),
        ]
