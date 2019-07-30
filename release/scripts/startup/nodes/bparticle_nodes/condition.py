import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleConditionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleConditionNode"
    bl_label = "Particle Condition"

    def declaration(self, builder: NodeBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.control_flow_output("if_true", "If True")
        builder.control_flow_output("if_false", "If False")
