import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder


class AlwaysExecuteNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_AlwaysExecuteNode"
    bl_label = "Always Execute"

    execute__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.execute_input("execute", "Execute", "execute__prop")
        builder.particle_effector_output("type", "Type")
