import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ExplodeParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ExplodeParticleNode"
    bl_label = "Explode Particle"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("speed", "Speed", "Float", default=2)
        builder.control_flow_output("control_out", "(Out)")
        builder.control_flow_output("new_control_out", "On Birth")

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
