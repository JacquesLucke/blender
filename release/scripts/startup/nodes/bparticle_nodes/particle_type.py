import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleTypeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTypeNode"
    bl_label = "Particle Type"

    def declaration(self, builder: NodeBuilder):
        builder.background_color((0.8, 0.5, 0.4))

        builder.emitter_input("emitters", "Emitters")
        builder.particle_effector_input("effectors", "Effectors")
        builder.event_output("events", "Events")

    def draw(self, layout):
        layout.prop(self, "name", text="", icon="MOD_PARTICLES")
