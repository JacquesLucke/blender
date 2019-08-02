import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ExplodeParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ExplodeParticleNode"
    bl_label = "Explode Particle"

    particle_type_name: StringProperty(maxlen=64)

    def declaration(self, builder: NodeBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("speed", "Speed", "Float", default=2)
        builder.control_flow_output("control_out", "(Out)")
        builder.control_flow_output("new_control_out", "On Birth")

    def draw(self, layout):
        row = layout.row(align=True)
        row.prop(self, "particle_type_name", text="", icon="MOD_PARTICLES")
        self.invoke_particle_type_creation(row, "on_type_created", "", icon='ADD')

    def on_type_created(self, new_node):
        new_node.name = "Exploded"
        self.particle_type_name = new_node.name
