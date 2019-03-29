import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    def get_sockets(self):
        return [
            FixedSocketDecl(self, "value", "Value", "Float"),
            FixedSocketDecl(self, "from_min", "From Min", "Float"),
            FixedSocketDecl(self, "from_max", "From Max", "Float"),
            FixedSocketDecl(self, "to_min", "To Min", "Float"),
            FixedSocketDecl(self, "to_max", "To Max", "Float"),
        ], [
            FixedSocketDecl(self, "value", "Value", "Float"),
        ]