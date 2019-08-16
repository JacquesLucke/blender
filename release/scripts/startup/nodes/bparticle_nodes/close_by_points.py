import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class CloseByPointsEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CloseByPointsEventNode"
    bl_label = "Close By Points Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("points", "Points", "Vector List")
        builder.fixed_input("distance", "Distance", "Float", default=1)
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.event_output("event", "Event")
