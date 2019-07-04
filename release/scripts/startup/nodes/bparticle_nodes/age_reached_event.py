import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    def declaration(self, builder : SocketBuilder):
        builder.event_input("events", "Event")
        builder.fixed_input("age", "Age", "Float")
