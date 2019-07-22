import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class TurbulenceForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_TurbulenceForceNode"
    bl_label = "Turbulence Force"

    def declaration(self, builder : NodeBuilder):
        builder.fixed_input("strength", "Strength", "Vector", default=(1, 1, 1))
        builder.particle_modifier_output("force", "Force")
