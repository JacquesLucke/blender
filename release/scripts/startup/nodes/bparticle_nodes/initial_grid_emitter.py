import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class InitialGridEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_InitialGridEmitterNode"
    bl_label = "Initial Grid Emitter"

    def declaration(self, builder : SocketBuilder):
        builder.fixed_input("amount_x", "Amount X", "Integer", default=10)
        builder.fixed_input("amount_y", "Amount Y", "Integer", default=10)
        builder.fixed_input("step_x", "Step X", "Float", default=0.2)
        builder.fixed_input("step_y", "Step Y", "Float", default=0.2)
        builder.fixed_input("size", "Size", "Float", default=0.01)
        builder.emitter_output("emitter", "Emitter")
