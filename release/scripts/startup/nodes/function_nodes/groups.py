import bpy
from bpy.props import *
from .. types import type_infos
from .. base import BaseNode
from .. function_tree import FunctionTree
from .. node_builder import NodeBuilder
from .. ui import NodeSidebarPanel

class GroupInputNode(BaseNode):
    sort_index: IntProperty()

class GroupDataInputNode(bpy.types.Node, GroupInputNode):
    bl_idname = "fn_GroupDataInputNode"
    bl_label = "Group Data Input"

    data_type: StringProperty(default="Float")

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("value", "Value", self.data_type)

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Select Type")

    def set_type(self, data_type):
        self.data_type = data_type
        self.sync_tree()


class GroupInterfacePanel(bpy.types.Panel, NodeSidebarPanel):
    bl_idname = "FN_PT_group_interface_panel"
    bl_label = "Group Interface"

    @classmethod
    def poll(self, context):
        try: return isinstance(context.space_data.edit_tree, FunctionTree)
        except: return False

    def draw(self, context):
        layout = self.layout
        tree = context.space_data.edit_tree

        input_nodes = [node for node in tree.nodes if isinstance(node, GroupDataInputNode)]
        input_nodes = sorted(input_nodes, key=lambda node: (node.sort_index, node.name))

        col = layout.column(align=True)
        for node in input_nodes:
            layout.label(text=node.name)
