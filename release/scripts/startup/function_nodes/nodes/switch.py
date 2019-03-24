import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class SwitchNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SwitchNode"
    bl_label = "Switch"

    data_type: StringProperty(
        default="Float",
        update=FunctionNode.refresh
    )

    def get_sockets(self):
        return [
            FixedSocketDecl("condition", "Condition", "Boolean"),
            FixedSocketDecl("true", "True", self.data_type),
            FixedSocketDecl("false", "False", self.data_type),
        ], [
            FixedSocketDecl("result", "Result", self.data_type),
        ]

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type")

    def set_type(self, data_type):
        self.data_type = data_type
