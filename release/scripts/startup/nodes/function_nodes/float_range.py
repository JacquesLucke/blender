import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class FloatRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatRangeNode"
    bl_label = "Float Range"

    mode: EnumProperty(
        name="Mode",
        items=[
            ("AMOUNT_START_STEP", "Amount / Start / Step", "", "NONE", 0),
            ("AMOUNT_START_STOP", "Amount / Start / Stop", "", "NONE", 1),
        ],
        default="AMOUNT_START_STOP",
        update=FunctionNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("amount", "Amount", "Integer", default=10)
        builder.fixed_input("start", "Start", "Float")
        if self.mode == "AMOUNT_START_STEP":
            builder.fixed_input("step", "Step", "Float")
        elif self.mode == "AMOUNT_START_STOP":
            builder.fixed_input("stop", "Stop", "Float", default=1)

        builder.fixed_output("list", "List", "Float List")

    def draw(self, layout):
        layout.prop(self, "mode", text="")
