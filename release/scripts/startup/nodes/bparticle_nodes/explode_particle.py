import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ExplodeParticleNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ExplodeParticleNode"
    bl_label = "Explode Particle"

    particle_type_name: StringProperty(maxlen=64)

    def declaration(self, builder : SocketBuilder):
        builder.control_flow_input("control_in", "(In)")
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("speed", "Speed", "Float", default=2)
        builder.control_flow_output("control_out", "(Out)")

    def draw(self, layout):
        layout.prop(self, "particle_type_name", text="Type")
