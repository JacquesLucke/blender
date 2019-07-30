import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    def declaration(self, builder: NodeBuilder):
        builder.event_input("event", "Event")
        builder.fixed_input("object", "Object", "Object")
        builder.control_flow_output("on_event", "On event")
