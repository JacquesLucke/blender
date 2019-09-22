import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleSystemNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleSystemNode"
    bl_label = "Particle System"

    def declaration(self, builder: NodeBuilder):
        builder.background_color((0.8, 0.5, 0.4))

        builder.influences_input("influences", "Influences")

    def draw(self, layout):
        layout.prop(self, "name", text="", icon="PHYSICS")
