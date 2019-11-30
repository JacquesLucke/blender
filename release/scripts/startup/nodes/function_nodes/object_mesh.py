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


class GetPositionOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetPositionOnSurfaceNode"
    bl_label = "Get Position on Surface"

    def declaration(self, builder):
        builder.fixed_input("location", "Location", "Surface Location")
        builder.fixed_output("position", "Position", "Vector")


class GetWeightOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetWeightOnSurfaceNode"
    bl_label = "Get Weight on Surface"

    vertex_group_name: StringProperty(
        name="Vertex Group Name",
        default="Group",
    )

    def declaration(self, builder):
        builder.fixed_input("location", "Location", "Surface Location")
        builder.fixed_output("weight", "Weight", "Float")
    
    def draw(self, layout):
        layout.prop(self, "vertex_group_name", text="", icon="GROUP_VERTEX")