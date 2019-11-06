import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class KillParticleNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_KillParticleNode"
    bl_label = "Kill Particle"

    def declaration(self, builder: NodeBuilder):
        builder.execute_output("execute", "Execute")
