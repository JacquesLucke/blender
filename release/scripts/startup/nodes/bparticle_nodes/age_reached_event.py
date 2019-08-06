import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("age", "Age", "Float", default=3)
        builder.control_flow_output("on_event", "On Event")

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
