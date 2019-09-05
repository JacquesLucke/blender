import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class SizeOverTimeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_SizeOverTimeNode"
    bl_label = "Size Over Time"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("final_size", "Final Size", "Float", default=0.0)
        builder.fixed_input("final_age", "Final Age", "Float", default=3)
        builder.particle_effector_output("type", "Type")
