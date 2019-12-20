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

    def draw_socket(self, layout, socket, text, decl, index_in_decl):
        row = layout.row(align=True)
        row.prop(self, "input_name", text="")
        row.prop(self, "display_settings", text="", icon="SETTINGS")

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
        update=BaseNode.sync_tree,
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

    def draw_socket(self, layout, socket, text, decl, index_in_decl):
        row = layout.row(align=True)
        row.prop(self, "output_name", text="")
        row.prop(self, "display_settings", text="", icon="SETTINGS")

    def draw_closed_label(self):
        return self.output_name + " (Output)"

    def set_type_type(self, data_type):
        self.data_type = data_type

class GroupNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GroupNode"
    bl_label = "Group"
    bl_icon = "NODETREE"

    node_group: PointerProperty(
        type=bpy.types.NodeTree,
        update=FunctionNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        if not isinstance(self.node_group, FunctionTree):
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
        layout.scale_y = 1.3
        if self.node_group is None:
            self.invoke_group_selector(layout, "set_group", "Select Group", icon="NODETREE")
        elif not isinstance(self.node_group, FunctionTree):
            layout.label(text="Group not found!", icon="ERROR")
            self.invoke_group_selector(layout, "set_group", "Change Group", icon="NODETREE")

    def draw_advanced(self, layout):
        col = layout.column()
        text = "Select Group" if self.node_group is None else self.node_group.name
        col.scale_y = 1.3
        self.invoke_group_selector(col, "set_group", text, icon="NODETREE")

    def draw_label(self):
        if self.node_group is None:
            return "(G) -"
        else:
            return "(G) " + self.node_group.name

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
        draw_group_interface_panel(layout, tree)


def draw_group_interface_panel(layout, tree):
    col = layout.column(align=True)
    col.label(text="Inputs:")
    box = col.box().column(align=True)
    for i, node in enumerate(tree.get_input_nodes()):
        row = box.row(align=True)
        row.prop(node, "input_name", text="")

        props = row.operator("fn.move_group_interface", text="", icon="TRIA_UP")
        props.is_input = True
        props.from_index = i
        props.offset = -1

        props = row.operator("fn.move_group_interface", text="", icon="TRIA_DOWN")
        props.is_input = True
        props.from_index = i
        props.offset = 1

    col = layout.column(align=True)
    col.label(text="Outputs:")
    box = col.box().column(align=True)
    for i, node in enumerate(tree.get_output_nodes()):
        row = box.row(align=True)
        row.prop(node, "output_name", text="")

        props = row.operator("fn.move_group_interface", text="", icon="TRIA_UP")
        props.is_input = False
        props.from_index = i
        props.offset = -1

        props = row.operator("fn.move_group_interface", text="", icon="TRIA_DOWN")
        props.is_input = False
        props.from_index = i
        props.offset = 1


class MoveGroupInterface(bpy.types.Operator):
    bl_idname = "fn.move_group_interface"
    bl_label = "Move Group Interface"

    is_input: BoolProperty()
    from_index: IntProperty()
    offset: IntProperty()

    def execute(self, context):
        tree = context.space_data.node_tree

        if self.is_input:
            nodes = tree.get_input_nodes()
        else:
            nodes = tree.get_output_nodes()

        from_index = self.from_index
        to_index = min(max(self.from_index + self.offset, 0), len(nodes) - 1)

        nodes[from_index], nodes[to_index] = nodes[to_index], nodes[from_index]

        with skip_syncing():
            for i, node in enumerate(nodes):
                node.sort_index = i
        tree.sync()

        return {"FINISHED"}


def update_sort_indices(tree):
    for i, node in enumerate(tree.get_input_nodes()):
        node.sort_index = i
    for i, node in enumerate(tree.get_output_nodes()):
        node.sort_index = i


class ManageGroupPieMenu(bpy.types.Menu, PieMenuHelper):
    bl_idname = "FN_MT_manage_group_pie"
    bl_label = "Manage Group"

    @classmethod
    def poll(cls, context):
        try:
            return isinstance(context.space_data.node_tree, FunctionTree)
        except:
            return False

    def draw_top(self, layout):
        layout.operator("fn.open_group_management_popup", text="Group Management")

    def draw_left(self, layout):
        node = bpy.context.active_node
        if node is None:
            self.empty(layout)
            return

        possible_inputs = [(i, socket) for i, socket in enumerate(node.inputs)
                                       if socket_can_become_group_input(socket)]

        if len(possible_inputs) == 0:
            self.empty(layout, "No inputs.")
        elif len(possible_inputs) == 1:
            props = layout.operator("fn.create_group_input_for_socket", text="New Group Input")
            props.input_index = possible_inputs[0][0]
        else:
            layout.operator("fn.create_group_input_for_socket_invoker", text="New Group Input")

    def draw_right(self, layout):
        node = bpy.context.active_node
        if node is None:
            self.empty(layout)
            return

        possible_outputs = [(i, socket) for i, socket in enumerate(node.outputs)
                                        if socket_can_become_group_output(socket)]

        if len(possible_outputs) == 0:
            self.empty(layout, "No outputs.")
        elif len(possible_outputs) == 1:
            props = layout.operator("fn.create_group_output_for_socket", text="New Group Output")
            props.output_index = possible_outputs[0][0]
        else:
            layout.operator("fn.create_group_output_for_socket_invoker", text="New Group Output")


class OpenGroupManagementPopup(bpy.types.Operator):
    bl_idname = "fn.open_group_management_popup"
    bl_label = "Group Management"

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def draw(self, context):
        draw_group_interface_panel(self.layout, context.space_data.node_tree)

    def execute(self, context):
        return {"INTERFACE"}


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


class CreateGroupOutputForSocketInvoker(bpy.types.Operator):
    bl_idname = "fn.create_group_output_for_socket_invoker"
    bl_label = "Create Group Output for Socket Invoker"

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

        for i, socket in enumerate(node.outputs):
            if socket_can_become_group_output(socket):
                props = layout.operator("fn.create_group_output_for_socket", text=socket.name)
                props.output_index = i


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
            update_sort_indices(tree)

            if isinstance(socket, DataSocket):
                new_node.interface_type = "DATA"
                new_node.data_type = socket.data_type
            elif socket.bl_idname == "fn_ExecuteSocket":
                new_node.interface_type = "EXECUTE"
            elif socket.bl_idname == "fn_InfluencesSocket":
                new_node.interface_type = "INFLUENCES"
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


class CreateGroupOutputForSocket(bpy.types.Operator):
    bl_idname = "fn.create_group_output_for_socket"
    bl_label = "Create Group Output for Socket"

    output_index: IntProperty()

    def invoke(self, context, event):
        tree = context.space_data.node_tree
        node = context.active_node
        socket = node.outputs[self.output_index]

        node.select = False

        with skip_syncing():
            new_node = tree.nodes.new(type="fn_GroupOutputNode")
            new_node.sort_index = 1000
            update_sort_indices(tree)

            new_node.output_name = socket.name
            if isinstance(socket, DataSocket):
                new_node.interface_type = "DATA"
                new_node.data_type = socket.data_type
            elif socket.bl_idname == "fn_ExecuteSocket":
                new_node.interface_type = "EXECUTE"
            elif socket.bl_idname == "fn_InfluencesSocket":
                new_node.interface_type = "INFLUENCES"
            new_node.rebuild()

            new_node.select = True
            new_node.parent = node.parent
            new_node.location = node.location
            new_node.location.x += 200

            tree.new_link(new_node.inputs[0], socket)

        tree.sync()
        bpy.ops.node.translate_attach("INVOKE_DEFAULT")
        return {"FINISHED"}


class OpenCloseGroupOperator(bpy.types.Operator):
    bl_idname = "fn.open_close_group"
    bl_label = "Open/Close Group"
    bl_options = {"INTERNAL"}

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "FunctionTree"
        except: return False

    def invoke(self, context, event):
        space_data = context.space_data
        active_node = context.active_node
        if isinstance(active_node, GroupNode) and active_node.node_group is not None and active_node.select:
            space_data.path.append(active_node.node_group, node=active_node)
        else:
            space_data.path.pop()
        return {"FINISHED"}


def socket_can_become_group_input(socket):
    return socket.bl_idname != "fn_OperatorSocket" and not socket.is_linked

def socket_can_become_group_output(socket):
    return socket.bl_idname != "fn_OperatorSocket"

keymap = None

def register():
    global keymap

    if not bpy.app.background:
        keymap = bpy.context.window_manager.keyconfigs.addon.keymaps.new(
            name="Node Editor", space_type="NODE_EDITOR")

        kmi = keymap.keymap_items.new("wm.call_menu_pie", type="V", value="PRESS")
        kmi.properties.name = "FN_MT_manage_group_pie"

        keymap.keymap_items.new("fn.open_close_group", type="TAB", value="PRESS")

def unregister():
    global keymap

    if not bpy.app.background:
        bpy.context.window_manager.keyconfigs.addon.keymaps.remove(keymap)
        keymap = None
