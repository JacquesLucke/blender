import bpy
from .. base import BaseNode
from .. socket_decl import AnyVariadicDecl

class FunctionInputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    variadic: AnyVariadicDecl.Property()

    def get_sockets(self):
        return [], [
        AnyVariadicDecl("outputs", "variadic", "New Input")
        ]
