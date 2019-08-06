import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class CloseByPointsEventNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CloseByPointsEventNode"
    bl_label = "Close By Points Event"

    particle_type: NodeBuilder.ParticleTypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("points", "Points", "Vector List")
        builder.fixed_input("distance", "Distance", "Float", default=1)
        builder.control_flow_output("on_event", "On Event")

    def draw(self, layout):
        NodeBuilder.draw_particle_type_prop(layout, self, "particle_type")

    def get_used_particle_type_names(self):
        return [self.particle_type]
