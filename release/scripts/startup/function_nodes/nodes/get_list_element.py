import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl, BaseSocketDecl, ListSocketDecl

class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: BaseSocketDecl.Property()

    def get_sockets(self):
        return [
            ListSocketDecl("list", "List", "active_type"),
            FixedSocketDecl("index", "Index", "Integer"),
            BaseSocketDecl("fallback", "Fallback", "active_type"),
        ], [
            BaseSocketDecl("value", "Value", "active_type"),
        ]