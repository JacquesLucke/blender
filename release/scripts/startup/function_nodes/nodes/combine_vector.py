import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class CombineVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineVectorNode"
    bl_label = "Combine Vector"

    def get_sockets(self):
        return [
            FixedSocketDecl(self, "x", "X", "Float"),
            FixedSocketDecl(self, "y", "Y", "Float"),
            FixedSocketDecl(self, "z", "Z", "Float"),
        ], [
            FixedSocketDecl(self, "result", "Result", "Vector"),
        ]