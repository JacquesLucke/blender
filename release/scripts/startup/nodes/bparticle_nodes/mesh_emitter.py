import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("rate", "Rate", "Float", default=10)
        builder.fixed_input("normal_velocity", "Normal Velocity", "Float", default=1)
        builder.fixed_input("emitter_velocity", "Emitter Velocity", "Float", default=0)
        builder.fixed_input("size", "Size", "Float", default=0.05)
        builder.emitter_output("emitter", "Emitter")
