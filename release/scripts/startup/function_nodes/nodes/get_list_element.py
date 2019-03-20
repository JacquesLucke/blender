import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl, ListSocketDecl

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: ListSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("list", "List", "active_type", "LIST"),
            FixedSocketDecl("index", "Index", "Integer"),
            ListSocketDecl("fallback", "Fallback", "active_type", "BASE"),
        ], [
            ListSocketDecl("value", "Value", "active_type", "BASE"),
        ]