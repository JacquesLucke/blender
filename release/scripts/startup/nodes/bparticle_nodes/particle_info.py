import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ParticleInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleInfoNode"
    bl_label = "Particle Info"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_output("position", "Position", "Vector")
        builder.fixed_output("velocity", "Velocity", "Vector")
        builder.fixed_output("birth_time", "Birth Time", "Float")
