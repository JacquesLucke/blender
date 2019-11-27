import bpy
from bpy.props import *
from .. base import FunctionNode

class ObjectMeshNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ObjectMeshNode"
    bl_label = "Object Mesh"

    def declaration(self, builder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_output("vertex_locations", "Vertex Locations", "Vector List")


class VertexInfo(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VertexInfoNode"
    bl_label = "Vertex Info"

    def declaration(self, builder):
        builder.fixed_output("position", "Position", "Vector")


class ClosestLocationOnObjectNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClosestLocationOnObjectNode"
    bl_label = "Closest Location on Object"

    def declaration(self, builder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("position", "Position", "Vector")
        builder.fixed_output("closest_point", "Closest Location", "Surface Location")
