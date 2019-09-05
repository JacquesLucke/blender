import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder


class ParticleInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleInfoNode"
    bl_label = "Particle Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("id", "ID", "Integer")
        builder.fixed_output("position", "Position", "Vector")
        builder.fixed_output("velocity", "Velocity", "Vector")
        builder.fixed_output("birth_time", "Birth Time", "Float")
        builder.fixed_output("age", "Age", "Float")


class SurfaceInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SurfaceInfoNode"
    bl_label = "Surface Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("normal", "Normal", "Vector")
        builder.fixed_output("velocity", "Velocity", "Vector")


class SurfaceImageNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SurfaceImageNode"
    bl_label = "Surface Image"

    uv_mode: EnumProperty(
        name="UV Mode",
        items=[
            ('FIRST', "First", "Use first UV map", 'NONE', 0),
            ('BY_NAME', "By Name", "Choose the UV map by name", 'NONE', 1),
        ],
        update=BParticlesNode.sync_tree,
        default='FIRST',
    )

    image: PointerProperty(type=bpy.types.Image)

    def declaration(self, builder: NodeBuilder):
        if self.uv_mode == 'BY_NAME':
            builder.fixed_input("uv_name", "Name", "Text")
        builder.fixed_output("color", "Color", "Color")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "image", text="")
        col.prop(self, "uv_mode", text="")


class SurfaceWeightNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SurfaceWeightNode"
    bl_label = "Surface Weight"

    group_name: StringProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("weight", "Weight", "Float")

    def draw(self, layout):
        layout.prop(self, "group_name", text="")
