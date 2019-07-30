import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleTrailsNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTrailsNode"
    bl_label = "Particle Trails"

    particle_type_name: StringProperty(maxlen=64)

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.particle_effector_output("effect", "Effect")

    def draw(self, layout):
        row = layout.row(align=True)
        row.prop(self, "particle_type_name", text="", icon="MOD_PARTICLES")
        self.invoke_particle_type_creation(row, "on_type_created", "", icon='ADD')

    def on_type_created(self, new_node):
        new_node.name = "Trails"
        self.particle_type_name = new_node.name
