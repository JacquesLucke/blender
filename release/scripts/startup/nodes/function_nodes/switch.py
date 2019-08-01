import bpy
from bpy.props import *
from .. base import FunctionNode

class SwitchNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SwitchNode"
    bl_label = "Switch"

    data_type: StringProperty(
        default="Float",
        update=FunctionNode.sync_tree
    )

    def declaration(self, builder):
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.fixed_input("true", "True", self.data_type)
        builder.fixed_input("false", "False", self.data_type)
        builder.fixed_output("result", "Result", self.data_type)

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type")

    def set_type(self, data_type):
        self.data_type = data_type
