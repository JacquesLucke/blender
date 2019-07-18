import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ParticleTrailsNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTrailsNode"
    bl_label = "Particle Trails"

    particle_type_name: StringProperty()

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=10)
        builder.particle_modifier_output("effect", "Effect")

    def draw(self, layout):
        layout.prop(self, "particle_type_name", text="")
