import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class TurbulenceForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_TurbulenceForceNode"
    bl_label = "Turbulence Force"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("strength", "Strength", "Vector", default=(1, 1, 1))

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
