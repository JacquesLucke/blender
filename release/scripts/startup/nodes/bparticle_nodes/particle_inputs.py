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


class CollisionInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CollisionInfoNode"
    bl_label = "Collision Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("normal", "Normal", "Vector")


class SurfaceImageNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SurfaceImageNode"
    bl_label = "Surface Image"

    image: PointerProperty(type=bpy.types.Image)

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("color", "Color", "Color")

    def draw(self, layout):
        layout.prop(self, "image", text="")


class SurfaceWeightNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SurfaceWeightNode"
    bl_label = "Surface Weight"

    group_name: StringProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("weight", "Weight", "Float")

    def draw(self, layout):
        layout.prop(self, "group_name", text="")
