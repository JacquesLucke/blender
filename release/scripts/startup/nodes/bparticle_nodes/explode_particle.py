import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class ExplodeParticleNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ExplodeParticleNode"
    bl_label = "Explode Particle"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("speed", "Speed", "Float", default=2)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")

        builder.execute_output("execute", "Execute")
        builder.influences_output("explode_system", "Explode System")
