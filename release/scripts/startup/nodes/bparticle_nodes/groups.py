import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class AddToGroupNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AddToGroupNode"
    bl_label = "Add to Group"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("group_name", "Group", "Text")
        builder.execute_output("execute", "Execute")


class RemoveFromGroupNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_RemoveFromGroupNode"
    bl_label = "Remove from Group"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("group_name", "Group", "Text")
        builder.execute_output("execute", "Execute")


class IsInGroupNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_IsInGroupNode"
    bl_label = "Is in Group"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("group_name", "Group", "Text")
        builder.fixed_output("is_in_group", "Is in Group", "Boolean")
