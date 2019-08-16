import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleConditionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleConditionNode"
    bl_label = "Particle Condition"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.control_flow_input("if_true", "Execute If True")
        builder.control_flow_input("if_false", "Execute If False")

        builder.control_flow_output("execute", "Execute")
