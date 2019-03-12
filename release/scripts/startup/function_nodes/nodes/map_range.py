import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    def get_sockets(self):
        return [
            FixedSocketDecl("Value", "Float"),
            FixedSocketDecl("From Min", "Float"),
            FixedSocketDecl("From Max", "Float"),
            FixedSocketDecl("To Min", "Float"),
            FixedSocketDecl("To Max", "Float"),
        ], [
            FixedSocketDecl("Value", "Float"),
        ]