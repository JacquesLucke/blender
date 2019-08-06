import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class PointEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointEmitterNode"
    bl_label = "Point Emitter"

    particle_type: BParticlesNode.TypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("position", "Position", "Vector")
        builder.fixed_input("velocity", "Velocity", "Vector", default=(1, 0, 0))
        builder.fixed_input("size", "Size", "Float", default=0.01)

    def draw(self, layout):
        self.draw_particle_type_selector(layout, "particle_type")

    def get_used_particle_types(self):
        return [self.particle_type]
