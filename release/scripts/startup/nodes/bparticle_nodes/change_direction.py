import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ChangeParticleDirectionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ChangeParticleDirectionNode"
    bl_label = "Change Particle Direction"

    def declaration(self, builder : SocketBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("direction", "Direction", "Vector")
        builder.control_flow_output("control_out", "(Out)")
