import bpy
from bpy.props import *
from .. types import type_infos
from .. base import BaseNode, FunctionNode
from .. function_tree import FunctionTree
from .. node_builder import NodeBuilder
from .. ui import NodeSidebarPanel

class GroupInputNode(BaseNode):
    sort_index: IntProperty()

class GroupOutputNode(BaseNode):
    sort_index: IntProperty()

class GroupDataInputNode(bpy.types.Node, GroupInputNode):
    bl_idname = "fn_GroupDataInputNode"
    bl_label = "Group Data Input"

    input_name: StringProperty(
        default="Name",
        update=GroupInputNode.sync_tree,
    )

    data_type: StringProperty(
        default="Float",
        update=GroupInputNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("value", "Value", self.data_type)

    def draw(self, layout):
        layout.prop(self, "input_name", text="")

        if hasattr(self.outputs[0], "draw_property"):
            self.outputs[0].draw_property(layout, self, "Default")

        self.invoke_type_selection(layout, "set_type", "Select Type")

    def set_type(self, data_type):
        self.data_type = data_type

class GroupDataOutputNode(bpy.types.Node, GroupOutputNode):
    bl_idname = "fn_GroupDataOutputNode"
    bl_label = "Group Data Output"

    output_name: StringProperty(
        default="Name",
        update=GroupOutputNode.sync_tree,
    )

    data_type: StringProperty(
        default="Float",
        update=GroupOutputNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("value", "Value", self.data_type)

    def draw(self, layout):
        layout.prop(self, "output_name", text="")
        self.invoke_type_selection(layout, "set_type", "Select Type")

    def set_type(self, data_type):
        self.data_type = data_type

class GroupNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GroupNode"
    bl_label = "Group"

    node_group: PointerProperty(
        type=bpy.types.NodeTree,
        update=FunctionNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        if self.node_group is None:
            return

        for input_node in self.node_group.get_input_nodes():
            builder.fixed_input(
                input_node.identifier,
                input_node.input_name,
                input_node.data_type,
                default=input_node.outputs[0].get_state())

        for output_node in self.node_group.get_output_nodes():
            builder.fixed_output(
                output_node.identifier,
                output_node.output_name,
                output_node.data_type)

    def draw(self, layout):
        text = "Select Group" if self.node_group is None else self.node_group.name
        layout.scale_y = 1.3
        self.invoke_group_selector(layout, "set_group", text, icon="NODETREE")

    def set_group(self, group):
        self.node_group = group

    def iter_directly_used_trees(self):
        if self.node_group is not None:
            yield self.node_group


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

        col = layout.column(align=True)
        col.label(text="Inputs:")
        for node in tree.get_input_nodes():
            layout.label(text=node.input_name)

        col = layout.column(align=True)
        col.label(text="Outputs:")
        for node in tree.get_output_nodes():
            layout.label(text=node.output_name)
