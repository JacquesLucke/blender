import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class KillParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_KillParticleNode"
    bl_label = "Kill Particle"

    def declaration(self, builder : NodeBuilder):
        builder.control_flow_input("control_in", "(In)")
