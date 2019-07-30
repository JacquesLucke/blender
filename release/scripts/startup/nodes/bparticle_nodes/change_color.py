import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ChangeParticleColorNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ChangeParticleColorNode"
    bl_label = "Change Particle Color"

    def declaration(self, builder: NodeBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("color", "Color", "Color")
        builder.control_flow_output("control_out", "(Out)")
