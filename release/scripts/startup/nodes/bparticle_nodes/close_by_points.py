import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class CloseByPointsEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CloseByPointsEventNode"
    bl_label = "Close By Points Event"

    def declaration(self, builder : SocketBuilder):
        builder.event_input("event", "Event")
        builder.fixed_input("points", "Points", "Vector List")
        builder.fixed_input("distance", "Distance", "Float", default=1)
        builder.control_flow_output("on_event", "On Event")
