import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class GravityForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_GravityForceNode"
    bl_label = "Gravity Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("direction", "Direction", "Vector", default=(0, 0, -1))
        builder.particle_modifier_output("force", "Force")
