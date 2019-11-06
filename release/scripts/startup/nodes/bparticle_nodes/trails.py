import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class ParticleTrailsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleTrailsNode"
    bl_label = "Particle Trails"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("main_system", "Main System")
        builder.influences_output("trail_system", "Trail System")
