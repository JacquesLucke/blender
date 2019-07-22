import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    def declaration(self, builder : NodeBuilder):
        builder.event_input("event", "Event")
        builder.fixed_input("age", "Age", "Float", default=3)
        builder.control_flow_output("on_event", "On Event")
