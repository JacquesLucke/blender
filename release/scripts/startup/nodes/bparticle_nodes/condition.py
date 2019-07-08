import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ParticleConditionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleConditionNode"
    bl_label = "Particle Condition"

    def declaration(self, builder : SocketBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.control_flow_output("if_true", "If True")
        builder.control_flow_output("if_false", "If False")
