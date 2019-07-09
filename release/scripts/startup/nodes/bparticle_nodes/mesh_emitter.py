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
        builder.emitter_output("emitter", "Emitter")
