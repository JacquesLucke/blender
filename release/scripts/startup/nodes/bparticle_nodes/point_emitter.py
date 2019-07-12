import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class PointEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointEmitterNode"
    bl_label = "Point Emitter"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("position", "Position", "Vector")
        builder.emitter_output("emitter", "Emitter")
