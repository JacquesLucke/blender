import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleInfoNode"
    bl_label = "Particle Info"

    def declaration(self, builder : NodeBuilder):
        builder.fixed_output("position", "Position", "Vector")
        builder.fixed_output("velocity", "Velocity", "Vector")
        builder.fixed_output("birth_time", "Birth Time", "Float")
        builder.fixed_output("age", "Age", "Float")
