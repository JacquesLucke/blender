import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class PointEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_PointEmitterNode"
    bl_label = "Point Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("position", "Position", "Vector")
        builder.fixed_input("velocity", "Velocity", "Vector", default=(1, 0, 0))
        builder.fixed_input("size", "Size", "Float", default=0.01)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("emitter", "Emitter")
