import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class GravityForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_GravityForceNode"
    bl_label = "Gravity Force"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("direction", "Direction", "Vector", default=(0, 0, -1))

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
