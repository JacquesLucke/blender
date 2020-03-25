import bpy
from bpy.props import *
from .. base import GeometryNode
from .. node_builder import NodeBuilder

mesh_element_items = [
    ("VERTEX", "Vertex", "", "VERTEXSEL", 0),
    ("EDGE", "Edge", "", "EDGESEL", 1),
    ("FACE", "Face", "", "FACESEL", 2),
]

class GeometryInputNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_GeometryInputNode"
    bl_label = "Geometry Input"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object", display_name=False)
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")

class PerlinNoiseNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_PerlinNoiseNode"
    bl_label = "Perlin Noise"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_input("strength", "Strength", "Float")
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_output("attribute", "Attribute", "Text")

class DisplaceSimpleNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_DisplaceSimpleNode"
    bl_label = "Displace Simple"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_input("vector", "Vector", "Vector")
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")

class DisplaceExpressionNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_DisplaceExpressionNode"
    bl_label = "Displace Expression"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_input("selection", "Selection", "Text")
        builder.fixed_input("vector", "Vector", "Text")
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")

class DisplaceVectorNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_DisplaceVectorNode"
    bl_label = "Displace Vector"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_input("selection", "Selection", "Text")
        builder.fixed_input("vector", "Vector", "Vector")
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")

class SetGeometryAttributeNode(bpy.types.Node, GeometryNode):
    bl_idname = "fn_SetGeometryAttributeNode"
    bl_label = "Set Geometry Attribute"

    mode: EnumProperty(items=mesh_element_items)

    def draw(self, layout):
        layout.prop(self, "mode", text="", expand=True)

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.fixed_input("attribute", "Attribute", "Text")
        builder.fixed_input("selection", "Selection", "Text")
        builder.fixed_input("value", "Value", "Text")
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")
