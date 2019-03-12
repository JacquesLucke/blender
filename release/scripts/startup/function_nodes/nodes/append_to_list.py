import bpy
from .. base import FunctionNode
from .. socket_decl import BaseSocketDecl, ListSocketDecl

class AppendToListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_AppendToListNode"
    bl_label = "Append to List"

    active_type: BaseSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("List", "active_type"),
            BaseSocketDecl("Value", "active_type"),
        ], [
            ListSocketDecl("List", "active_type"),
        ]