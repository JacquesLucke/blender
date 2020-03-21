import bpy
from bpy.props import *
from .. base import GeometryNode
from .. node_builder import NodeBuilder

class GeometryInputNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_GeometryInputNode"
    bl_label = "Geometry Input"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object", display_name=False)
        builder.mockup_output("geometry", "Geometry", "fn_MeshSocket")

class PerlinNoiseNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_PerlinNoiseNode"
    bl_label = "Perlin Noise"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_MeshSocket")
        builder.fixed_input("strength", "Strength", "Float")
        builder.mockup_output("geometry", "Geometry", "fn_MeshSocket")
        builder.fixed_output("attribute", "Attribute", "Text")

class DisplaceNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_DisplaceNode"
    bl_label = "Displace"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_MeshSocket")
        builder.fixed_input("strength", "Strength", "Text")
        builder.mockup_output("geometry", "Geometry", "fn_MeshSocket")
