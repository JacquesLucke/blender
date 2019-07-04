import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    age: FloatProperty(name="Age", default=3)

    def declaration(self, builder : SocketBuilder):
        builder.event_input("events", "Event")

    def draw(self, layout):
        layout.prop(self, "age", text="Age")
