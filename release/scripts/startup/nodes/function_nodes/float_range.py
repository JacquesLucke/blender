import bpy
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class FloatRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatRangeNode"
    bl_label = "Float Range"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount", "Amount", "Integer")
        builder.fixed_input("start", "Start", "Float")
        builder.fixed_input("step", "Step", "Float")

        builder.fixed_output("list", "List", "Float List")
