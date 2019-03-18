import bpy
from .. base import BaseNode
from .. socket_decl import AnyVariadicDecl

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    variadic: AnyVariadicDecl.Property()

    def get_sockets(self):
        return [
            AnyVariadicDecl("inputs", "variadic", "New Output"),
        ], []