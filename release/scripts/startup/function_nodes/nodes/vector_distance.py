import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class VectorDistanceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorDistanceNode"
    bl_label = "Vector Distance"

    def get_sockets(self):
        return [
            FixedSocketDecl("a", "A", "Vector"),
            FixedSocketDecl("b", "B", "Vector"),
        ], [
            FixedSocketDecl("distance", "Distance", "Float"),
        ]
