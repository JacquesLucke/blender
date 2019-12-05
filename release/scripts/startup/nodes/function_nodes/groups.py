import bpy
from bpy.props import *
from .. types import type_infos
from .. base import BaseNode, FunctionNode, DataSocket
from .. function_tree import FunctionTree
from .. node_builder import NodeBuilder
from .. ui import NodeSidebarPanel
from .. utils.pie_menu_helper import PieMenuHelper
from .. sync import skip_syncing

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

    display_settings: BoolProperty(
        name="Display Settings",
        default=False,
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
        if self.interface_type == "DATA":
            builder.fixed_output("value", "Value", self.data_type)
        elif self.interface_type == "EXECUTE":
            builder.execute_output("execute", "Execute")
        elif self.interface_type == "INFLUENCES":
            builder.influences_output("influences", "Influences")
        else:
            assert False

    def draw(self, layout):
        if not self.display_settings:
            return

        layout.prop(self, "interface_type", text="")

        if self.interface_type == "DATA":
            if hasattr(self.outputs[0], "draw_property"):
                self.outputs[0].draw_property(layout, self, "Default")

            self.invoke_type_selection(layout, "set_data_type", "Select Type")

    def draw_socket(self, layout, socket, text, decl, index):
        row = layout.row(align=True)
        row.prop(self, "display_settings", text="", icon="SETTINGS")
        row.prop(self, "input_name", text="")

    def draw_closed_label(self):
        return self.input_name + " (Input)"

    def set_data_type(self, data_type):
        self.data_type = data_type


class GroupOutputNode(bpy.types.Node, BaseNode):
    bl_idname = "fn_GroupOutputNode"
    bl_label = "Group Output"

    sort_index: IntProperty()

    display_settings: BoolProperty(
        name="Display Settings",
        default=False,
    )

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
        if self.interface_type == "DATA":
            builder.fixed_input("value", "Value", self.data_type)
        elif self.interface_type == "EXECUTE":
            builder.single_execute_input("execute", "Execute")
        elif self.interface_type == "INFLUENCES":
            builder.influences_input("influences", "Influences")

    def draw(self, layout):
        if not self.display_settings:
            return

        layout.prop(self, "interface_type", text="")

        if self.interface_type == "DATA":
            self.invoke_type_selection(layout, "set_type_type", "Select Type")

    def draw_socket(self, layout, socket, text, decl, index):
        row = layout.row(align=True)
        row.prop(self, "display_settings", text="", icon="SETTINGS")
        row.prop(self, "output_name", text="")

    def draw_closed_label(self):
        return self.output_name + " (Output)"

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

    def draw_closed_label(self):
        return self.node_group.name

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


class ManageGroupPieMenu(bpy.types.Menu, PieMenuHelper):
    bl_idname = "FN_MT_manage_group_pie"
    bl_label = "Manage Group"

    @classmethod
    def poll(cls, context):
        try: 
            return isinstance(context.space_data.node_tree, FunctionTree)
        except:
            return False

    def draw_left(self, layout):
        node = bpy.context.active_node
        possible_inputs = [(i, socket) for i, socket in enumerate(node.inputs) 
                                       if socket_can_become_group_input(socket)]

        if len(possible_inputs) == 0:
            self.empty(layout, "No inputs.")
        elif len(possible_inputs) == 1:
            props = layout.operator("fn.create_group_input_for_socket", text="New Group Input")
            props.input_index = possible_inputs[0][0]
        else:
            layout.operator("fn.create_group_input_for_socket_invoker", text="New Group Input")


class CreateGroupInputForSocketInvoker(bpy.types.Operator):
    bl_idname = "fn.create_group_input_for_socket_invoker"
    bl_label = "Create Group Input for Socket Invoker"

    def invoke(self, context, event):
        context.window_manager.popup_menu(self.draw_menu)
        return {"CANCELLED"}

    @staticmethod
    def draw_menu(menu, context):
        node = bpy.context.active_node
        if node is None:
            return

        layout = menu.layout.column()
        layout.operator_context = "INVOKE_DEFAULT"

        for i, socket in enumerate(node.inputs):
            if socket_can_become_group_input(socket):
                props = layout.operator("fn.create_group_input_for_socket", text=socket.name)
                props.input_index = i


class CreateGroupInputForSocket(bpy.types.Operator):
    bl_idname = "fn.create_group_input_for_socket"
    bl_label = "Create Group Input for Socket"

    input_index: IntProperty()

    def invoke(self, context, event):
        tree = context.space_data.node_tree
        node = context.active_node
        socket = node.inputs[self.input_index]

        node.select = False

        with skip_syncing():
            new_node = tree.nodes.new(type="fn_GroupInputNode")
            new_node.sort_index = 1000
            new_node.input_name = socket.name
            new_node.interface_type = "DATA"
            new_node.data_type = socket.data_type
            new_node.rebuild()
            
            new_node.select = True
            new_node.parent = node.parent
            new_node.location = node.location
            new_node.location.x -= 200

            new_node.outputs[0].restore_state(socket.get_state())
            tree.new_link(new_node.outputs[0], socket)

        tree.sync()
        bpy.ops.node.translate_attach("INVOKE_DEFAULT")
        return {"FINISHED"}


def socket_can_become_group_input(socket):
    return socket.bl_idname != "fn_OperatorSocket" and not socket.is_linked


keymap = None

def register():
    global keymap
    keymap = bpy.context.window_manager.keyconfigs.addon.keymaps.new(
        name="Node Editor", space_type="NODE_EDITOR")

    kmi = keymap.keymap_items.new("wm.call_menu_pie", type="V", value="PRESS")
    kmi.properties.name = "FN_MT_manage_group_pie"

def unregister():
    global keymap
    bpy.context.window_manager.keyconfigs.addon.keymaps.remove(keymap)
    keymap = None