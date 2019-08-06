import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    particle_type: BParticlesNode.TypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.control_flow_output("on_event", "On event")

    def draw(self, layout):
        self.draw_particle_type_selector(layout, "particle_type")

    def get_used_particle_types(self):
        return [self.particle_type]
