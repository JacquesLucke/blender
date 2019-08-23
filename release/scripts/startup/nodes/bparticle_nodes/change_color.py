import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ChangeParticleColorNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ChangeParticleColorNode"
    bl_label = "Change Particle Color"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("color", "Color", "Color")
        builder.execute_output("execute", "Execute")
