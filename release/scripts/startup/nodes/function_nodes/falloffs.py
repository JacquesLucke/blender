import bpy
from .. node_builder import NodeBuilder
from .. base import FunctionNode

class PointDistanceFalloffNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PointDistanceFalloffNode"
    bl_label = "Point Distance Falloff"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("point", "Point", "Vector")
        builder.fixed_input("min_distance", "Min Distance", "Float")
        builder.fixed_input("max_distance", "Max Distance", "Float", default=1)
        builder.fixed_output("falloff", "Falloff", "Falloff")
