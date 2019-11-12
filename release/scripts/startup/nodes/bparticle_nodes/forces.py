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


class TurbulenceForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_TurbulenceForceNode"
    bl_label = "Turbulence Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("strength", "Strength", "Vector", default=(1, 1, 1))
        builder.fixed_input("size", "Size", "Float", default=0.5)
        builder.fixed_input("weight", "Weight", "Float", default=1)
        builder.influences_output("force", "Force")


class GravityForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_GravityForceNode"
    bl_label = "Gravity Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("acceleration", "Acceleration", "Vector", default=(0, 0, -1))
        builder.fixed_input("weight", "Weight", "Float", default=1)
        builder.influences_output("force", "Force")


class DragForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_DragForceNode"
    bl_label = "Drag Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("strength", "Strength", "Float", default=1)
        builder.fixed_input("weight", "Weight", "Float", default=1)
        builder.influences_output("force", "Force")


class MeshForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MeshForceNode"
    bl_label = "Mesh Force"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("strength", "Strength", "Float", default=1)
        builder.influences_output("force", "Force")
