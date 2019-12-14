import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class AlwaysExecuteNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AlwaysExecuteNode"
    bl_label = "Always Execute"

    execute__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.execute_input("execute", "Execute", "execute__prop")
        builder.influences_output("influence", "Influence")


class MultiExecuteNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MultiExecuteNode"
    bl_label = "Multi Execute"

    execute__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.execute_input("execute", "Execute", "execute__prop")
        builder.execute_output("execute", "Execute")
