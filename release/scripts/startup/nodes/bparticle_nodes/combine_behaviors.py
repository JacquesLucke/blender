import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class CombineBehaviorsNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CombineBehaviorsNode"
    bl_label = "Combine Behaviors"

    def declaration(self, builder: NodeBuilder):
        builder.particle_effector_input("behaviors", "Behaviors")
        builder.particle_effector_output("behaviors", "Behaviors")
