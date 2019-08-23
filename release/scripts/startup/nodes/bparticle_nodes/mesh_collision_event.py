import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.particle_effector_output("event", "Event")
