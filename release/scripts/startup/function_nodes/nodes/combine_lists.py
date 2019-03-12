import bpy
from .. base import FunctionNode
from .. socket_decl import ListSocketDecl

class CombineListsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineListsNode"
    bl_label = "Combine Lists"

    active_type: ListSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("List 1", "active_type"),
            ListSocketDecl("List 2", "active_type"),
        ], [
            ListSocketDecl("List", "active_type"),
        ]