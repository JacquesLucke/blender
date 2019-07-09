import bpy
from bpy.props import *
from .. base import FunctionNode

class ObjectTransformsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ObjectTransformsNode"
    bl_label = "Object Transforms"

    def declaration(self, builder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_output("location", "Location", "Vector")
