import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class ForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ForceNode"
    bl_label = "Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("force", "Force", "Vector")
        builder.influences_output("force", "Force")
