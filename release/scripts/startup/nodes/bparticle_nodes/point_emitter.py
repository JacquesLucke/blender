import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class PointEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointEmitterNode"
    bl_label = "Point Emitter"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("position", "Position", "Vector")
        builder.fixed_input("velocity", "Velocity", "Vector", default=(1, 0, 0))
        builder.fixed_input("size", "Size", "Float", default=0.01)
        builder.emitter_output("emitter", "Emitter")
