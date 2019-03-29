import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl, ListSocketDecl

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: ListSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl(self, "list", "List", "active_type", "LIST"),
            FixedSocketDecl(self, "index", "Index", "Integer"),
            ListSocketDecl(self, "fallback", "Fallback", "active_type", "BASE"),
        ], [
            ListSocketDecl(self, "value", "Value", "active_type", "BASE"),
        ]