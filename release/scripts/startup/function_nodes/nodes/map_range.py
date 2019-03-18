import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    def get_sockets(self):
        return [
            FixedSocketDecl("value", "Value", "Float"),
            FixedSocketDecl("from_min", "From Min", "Float"),
            FixedSocketDecl("from_max", "From Max", "Float"),
            FixedSocketDecl("to_min", "To Min", "Float"),
            FixedSocketDecl("to_max", "To Max", "Float"),
        ], [
            FixedSocketDecl("value", "Value", "Float"),
        ]