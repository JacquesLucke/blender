import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class InitialGridEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_InitialGridEmitterNode"
    bl_label = "Initial Grid Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount_x", "Amount X", "Integer", default=50)
        builder.fixed_input("amount_y", "Amount Y", "Integer", default=50)
        builder.fixed_input("step_x", "Step X", "Float", default=0.2)
        builder.fixed_input("step_y", "Step Y", "Float", default=0.2)
        builder.fixed_input("size", "Size", "Float", default=0.01)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("emitter", "Emitter")
