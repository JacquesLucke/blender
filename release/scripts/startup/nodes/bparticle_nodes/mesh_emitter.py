import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    particle_type: BParticlesNode.TypeProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("rate", "Rate", "Float", default=10)
        builder.fixed_input("normal_velocity", "Normal Velocity", "Float", default=1)
        builder.fixed_input("emitter_velocity", "Emitter Velocity", "Float", default=0)
        builder.fixed_input("size", "Size", "Float", default=0.05)
        builder.control_flow_output("on_birth", "On Birth")

    def draw(self, layout):
        self.draw_particle_type_selector(layout, "particle_type")

    def get_used_particle_types(self):
        return [self.particle_type]
