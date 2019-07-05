import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class KillParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_KillParticleNode"
    bl_label = "Kill Particle"

    def declaration(self, builder : SocketBuilder):
        builder.control_flow_input("control_in", "(In)")
