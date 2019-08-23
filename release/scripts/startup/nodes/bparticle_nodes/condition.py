import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleConditionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleConditionNode"
    bl_label = "Particle Condition"

    execute_if_true__prop: NodeBuilder.ExecuteInputProperty()
    execute_if_false__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.execute_input("execute_if_true", "Execute If True", "execute_if_true__prop")
        builder.execute_input("execute_if_false", "Execute If False", "execute_if_false__prop")

        builder.execute_output("execute", "Execute")
