import bpy
from .. base import FunctionNode
from .. socket_decl import ListSocketDecl

class AppendToListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_AppendToListNode"
    bl_label = "Append to List"

    active_type: ListSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("list", "List", "active_type", "LIST"),
            ListSocketDecl("value", "Value", "active_type", "BASE"),
        ], [
            ListSocketDecl("list", "List", "active_type", "LIST"),
        ]