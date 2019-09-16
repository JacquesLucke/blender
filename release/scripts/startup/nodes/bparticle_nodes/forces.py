import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder


class TurbulenceForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_TurbulenceForceNode"
    bl_label = "Turbulence Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("strength", "Strength", "Vector", default=(1, 1, 1))
        builder.fixed_input("size", "Size", "Float", default=0.5)
        builder.fixed_input("falloff", "Falloff", "Falloff")
        builder.influences_output("force", "Force")


class GravityForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_GravityForceNode"
    bl_label = "Gravity Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("direction", "Direction", "Vector", default=(0, 0, -1))
        builder.fixed_input("falloff", "Falloff", "Falloff")
        builder.influences_output("force", "Force")


class DragForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_DragForceNode"
    bl_label = "Drag Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("strength", "Strength", "Float", default=1)
        builder.fixed_input("falloff", "Falloff", "Falloff")
        builder.influences_output("force", "Force")


class MeshForceNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshForceNode"
    bl_label = "Mesh Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("strength", "Strength", "Float", default=1)
        builder.influences_output("force", "Force")
