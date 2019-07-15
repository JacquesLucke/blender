import bpy
from .. base import FunctionNode

class RandomNumberNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_RandomNumberNode"
    bl_label = "Random Number"

    def declaration(self, builder):
        builder.fixed_input("seed", "Seed", "Float")
        builder.fixed_input("min", "Min", "Float")
        builder.fixed_input("max", "Max", "Float", default=1.0)
        builder.fixed_output("value", "Value", "Float")
