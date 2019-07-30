import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class PointForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointForceNode"
    bl_label = "Point Force"

    def declaration(self, builder : NodeBuilder):
        builder.fixed_input("direction", "Direction", "Vector", default=(0, 0, -1))
        builder.fixed_input("strength", "Strength", "Float", default = 1.0)
        builder.fixed_input("falloff", "Falloff", "Float", default = 1.0)
        builder.fixed_input("distance", "Distance", "Float", default = 1.0)
        builder.fixed_input("gravitation", "Gravitation", "Boolean", default=False)
        builder.particle_effector_output("force", "Force")
