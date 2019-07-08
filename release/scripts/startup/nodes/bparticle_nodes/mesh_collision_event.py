import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    object: PointerProperty(
        name="Object",
        type=bpy.types.Object,
    )

    def declaration(self, builder : SocketBuilder):
        builder.event_input("event", "Event")
        builder.control_flow_output("on_event", "On event")
        builder.fixed_output("normal", "Normal", "Vector")

    def draw(self, layout):
        layout.prop(self, "object", text="")
