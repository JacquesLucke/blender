import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class GravityForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_GravityForceNode"
    bl_label = "Gravity Force"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("direction", "Direction", "Vector", default=(0, 0, -1))
        builder.particle_modifier_output("force", "Force")
