import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class PointEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointEmitterNode"
    bl_label = "Point Emitter"

    position: FloatVectorProperty(
        name="Value",
        size=3,
        default=(0.0, 0.0, 0.0),
    )

    def declaration(self, builder : SocketBuilder):
        builder.emitter_output("emitter", "Emitter")

    def draw(self, layout):
        layout.column().prop(self, "position", text="Position")
