import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshCollisionEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.control_flow_output("on_event", "On event")

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
