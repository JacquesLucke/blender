import bpy
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    def declaration(self, builder : SocketBuilder):
        builder.emitter_output("emitter", "Emitter")
