import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class VectorDistanceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorDistanceNode"
    bl_label = "Vector Distance"

    def get_sockets(self):
        return [
            FixedSocketDecl("A", "Vector"),
            FixedSocketDecl("B", "Vector"),
        ], [
            FixedSocketDecl("Distance", "Float"),
        ]
