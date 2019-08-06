import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class InitialGridEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_InitialGridEmitterNode"
    bl_label = "Initial Grid Emitter"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount_x", "Amount X", "Integer", default=10)
        builder.fixed_input("amount_y", "Amount Y", "Integer", default=10)
        builder.fixed_input("step_x", "Step X", "Float", default=0.2)
        builder.fixed_input("step_y", "Step Y", "Float", default=0.2)
        builder.fixed_input("size", "Size", "Float", default=0.01)

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
