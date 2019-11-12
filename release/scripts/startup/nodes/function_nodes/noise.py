import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class PerlinNoiseNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PerlinNoiseNode"
    bl_label = "Perlin Noise"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("position", "Position", "Vector")
        builder.fixed_input("amplitude", "Amplitude", "Float", default=1)
        builder.fixed_input("scale", "Scale", "Float", default=1)
        builder.fixed_output("noise", "Noise", "Float")
