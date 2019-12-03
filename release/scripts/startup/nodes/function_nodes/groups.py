import bpy
from bpy.props import *
from .. types import type_infos
from .. base import BaseNode, FunctionNode
from .. function_tree import FunctionTree
from .. node_builder import NodeBuilder
from .. ui import NodeSidebarPanel

interface_type_items = [
    ("DATA", "Data", "Some data type like integer or vector", "NONE", 0),
    ("EXECUTE", "Control Flow", "", "NONE", 1),
    ("INFLUENCES", "Influences", "", "NONE", 2),
]

class GroupInputNode(bpy.types.Node, BaseNode):
    bl_idname = "fn_GroupInputNode"
    bl_label = "Group Input"

    input_name: StringProperty(
        default="Name",
        update=BaseNode.sync_tree,
    )

    sort_index: IntProperty()

    interface_type: EnumProperty(
        items=interface_type_items,
        default="DATA",
        update= BaseNode.sync_tree,
    )

    data_type: StringProperty(
        default="Float",
        update=BaseNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.background_color((0.8, 0.8, 0.8))

        if self.interface_type == "DATA":
            builder.fixed_output("value", "Value", self.data_type)
        elif self.interface_type == "EXECUTE":
            builder.execute_output("execute", "Execute")
        elif self.interface_type == "INFLUENCES":
            builder.influences_output("influences", "Influences")
        else:
            assert False

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "input_name", text="")

        if self.interface_type == "DATA":
            if hasattr(self.outputs[0], "draw_property"):
                self.outputs[0].draw_property(col, self, "Default")

            self.invoke_type_selection(col, "set_data_type", "Select Type")

    def draw_advanced(self, layout):
        layout.prop(self, "interface_type", text="")

    def set_data_type(self, data_type):
        self.data_type = data_type


class GroupOutputNode(bpy.types.Node, BaseNode):
    bl_idname = "fn_GroupOutputNode"
    bl_label = "Group Output"

    sort_index: IntProperty()

    output_name: StringProperty(
        default="Name",
        update=BaseNode.sync_tree,
    )

    interface_type: EnumProperty(
        items=interface_type_items,
        default="DATA",
        update= BaseNode.sync_tree,
    )

    data_type: StringProperty(
        default="Float",
        update=BaseNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.background_color((0.8, 0.8, 0.8))

        if self.interface_type == "DATA":
            builder.fixed_input("value", "Value", self.data_type)
        elif self.interface_type == "EXECUTE":
            builder.single_execute_input("execute", "Execute")
        elif self.interface_type == "INFLUENCES":
            builder.influences_input("influences", "Influences")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "output_name", text="")

        if self.interface_type == "DATA":
            self.invoke_type_selection(col, "set_type_type", "Select Type")

    def draw_advanced(self, layout):
        layout.prop(self, "interface_type", text="")

    def set_type_type(self, data_type):
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
            if input_node.interface_type == "DATA":
                builder.fixed_input(
                    input_node.identifier,
                    input_node.input_name,
                    input_node.data_type,
                    default=input_node.outputs[0].get_state())
            elif input_node.interface_type == "EXECUTE":
                builder.single_execute_input(input_node.identifier, input_node.input_name)
            elif input_node.interface_type == "INFLUENCES":
                builder.influences_input(input_node.identifier, input_node.input_name)
            else:
                assert False

        for output_node in self.node_group.get_output_nodes():
            if output_node.interface_type == "DATA":
                builder.fixed_output(
                    output_node.identifier,
                    output_node.output_name,
                    output_node.data_type)
            elif output_node.interface_type == "EXECUTE":
                builder.execute_output(output_node.identifier, output_node.output_name)
            elif output_node.interface_type == "INFLUENCES":
                builder.influences_output(output_node.identifier, output_node.output_name)
            else:
                assert False

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
