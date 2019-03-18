import bpy
from .. base import FunctionNode
from .. socket_decl import BaseSocketDecl, ListSocketDecl

class AppendToListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_AppendToListNode"
    bl_label = "Append to List"

    active_type: BaseSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("list", "List", "active_type"),
            BaseSocketDecl("value", "Value", "active_type"),
        ], [
            ListSocketDecl("list", "List", "active_type"),
        ]