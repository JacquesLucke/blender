import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    object: PointerProperty(
        name="Object",
        type=bpy.types.Object,
    )

    def declaration(self, builder : SocketBuilder):
        builder.emitter_output("emitter", "Emitter")

    def draw(self, layout):
        layout.prop(self, "object", text="")
