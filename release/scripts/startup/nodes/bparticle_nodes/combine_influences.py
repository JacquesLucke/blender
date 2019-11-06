import bpy
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class CombineInfluencesNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CombineInfluencesNode"
    bl_label = "Combine Influences"

    def declaration(self, builder: NodeBuilder):
        builder.influences_input("influences", "Influences")
        builder.influences_output("influences", "Influences")
