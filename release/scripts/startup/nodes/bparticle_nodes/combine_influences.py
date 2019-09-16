import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class CombineInfluencesNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CombineInfluencesNode"
    bl_label = "Combine Influences"

    def declaration(self, builder: NodeBuilder):
        builder.influences_input("influences", "Influences")
        builder.influences_output("influences", "Influences")
