import bpy
from bpy.props import *
from .. base import FunctionNode

class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    def declaration(self, builder):
        builder.fixed_input("value", "Value", "Float")
        builder.fixed_input("from_min", "From Min", "Float", default=0)
        builder.fixed_input("from_max", "From Max", "Float", default=1)
        builder.fixed_input("to_min", "To Min", "Float", default=0)
        builder.fixed_input("to_max", "To Max", "Float", default=1)
        builder.fixed_output("value", "Value", "Float")
