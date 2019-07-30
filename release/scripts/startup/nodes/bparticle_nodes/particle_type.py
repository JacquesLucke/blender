import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleTypeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTypeNode"
    bl_label = "Particle Type"

    def declaration(self, builder: NodeBuilder):
        builder.emitter_input("emitters", "Emitters")
        builder.particle_modifier_input("effectors", "Effectors")
        builder.event_output("events", "Events")

    def draw(self, layout):
        layout.prop(self, "name", text="", icon="MOD_PARTICLES")
