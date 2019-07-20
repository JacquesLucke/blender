import bpy
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ParticleTypeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTypeNode"
    bl_label = "Particle Type"

    def declaration(self, builder : SocketBuilder):
        builder.emitter_input("emitters", "Emitters")
        builder.particle_modifier_input("effectors", "Effectors")
        builder.event_output("events", "Events")

    def draw(self, layout):
        layout.prop(self, "name", text="", icon="MOD_PARTICLES")
