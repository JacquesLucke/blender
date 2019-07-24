import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    object: PointerProperty(
        name="Object",
        type=bpy.types.Object,
    )

    def declaration(self, builder : NodeBuilder):
        builder.event_input("event", "Event")
        builder.control_flow_output("on_event", "On event")

    def draw(self, layout):
        layout.prop(self, "object", text="")
