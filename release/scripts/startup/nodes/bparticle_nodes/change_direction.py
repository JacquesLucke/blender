import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ChangeParticleDirectionNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ChangeParticleDirectionNode"
    bl_label = "Change Particle Direction"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("direction", "Direction", "Vector")
        builder.execute_output("execute", "Execute")
