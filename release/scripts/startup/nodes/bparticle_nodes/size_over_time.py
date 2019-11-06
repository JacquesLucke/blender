import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class SizeOverTimeNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SizeOverTimeNode"
    bl_label = "Size Over Time"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("final_size", "Final Size", "Float", default=0.0)
        builder.fixed_input("final_age", "Final Age", "Float", default=3)
        builder.influences_output("influence", "Influence")
