import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleTrailsNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTrailsNode"
    bl_label = "Particle Trails"

    source_particle_type: BParticlesNode.TypeProperty()
    trail_particle_type: BParticlesNode.TypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=20)

    def draw(self, layout):
        self.draw_particle_type_selector(layout, "source_particle_type", text="Source")
        self.draw_particle_type_selector(layout, "trail_particle_type", text="Trail")

    def get_used_particle_types(self):
        return [self.source_particle_type, self.trail_particle_type]
