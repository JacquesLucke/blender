import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder


class ValueNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ValueNode"
    bl_label = "Value"

    data_type = StringProperty(
        name="Data Type",
        default="Float",
        update=FunctionNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("value", "Value", self.data_type)

    def draw_socket(self, layout, socket, text, decl, index):
        row = layout.row(align=True)
        if hasattr(socket, "draw_property"):
            socket.draw_property(row, self, "")
        else:
            row.label(text=text)
        self.invoke_type_selection(row, "set_type", text="", icon="SETTINGS")

    def set_type(self, data_type):
        self.data_type = data_type
