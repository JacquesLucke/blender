import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ExplodeParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ExplodeParticleNode"
    bl_label = "Explode Particle"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("speed", "Speed", "Float", default=2)
        builder.control_flow_input("execute_on_birth", "Execute on Birth")

        builder.control_flow_output("execute", "Execute")
        builder.particle_effector_output("type", "Type")
