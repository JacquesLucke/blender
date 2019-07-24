import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class CollisionInfoNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_CollisionInfoNode"
    bl_label = "Collision Info"

    def declaration(self, builder : NodeBuilder):
        builder.fixed_output("normal", "Normal", "Vector")
