import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class AgeReachedEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    particle_type: BParticlesNode.TypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("age", "Age", "Float", default=3)
        builder.control_flow_output("on_event", "On Event")

    def draw(self, layout):
        self.draw_particle_type_selector(layout, "particle_type")

    def get_used_particle_types(self):
        return [self.particle_type]
