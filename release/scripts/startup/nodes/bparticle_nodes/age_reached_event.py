import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("age", "Age", "Float", default=3)
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.particle_effector_output("event", "Event")
