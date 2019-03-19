import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import VariadicListDecl, FixedSocketDecl
from .. sockets import type_infos

class CreateListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CreateListNode"
    bl_label = "Create List"

    active_type: StringProperty(default="Float")
    variadic: VariadicListDecl.Property()

    def get_sockets(self):
        return [
            VariadicListDecl("inputs", "variadic", self.active_type),
        ], [
            FixedSocketDecl("output", "List", type_infos.to_list(self.active_type)),
        ]