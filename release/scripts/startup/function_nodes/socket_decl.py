import bpy
from bpy.props import *
from dataclasses import dataclass
from . sockets import type_infos, OperatorSocket
from . base import DataSocket
import uuid

class SocketDeclBase:
    def build(self, node, node_sockets):
        raise NotImplementedError()

    def amount(self, node):
        raise NotImplementedError()

    def draw_socket(self, layout, node, socket, index):
        socket.draw_self(layout, self)

    def operator_socket_call(self, node, own_socket, other_socket):
        pass

class FixedSocketDecl(SocketDeclBase):
    def __init__(self, identifier: str, display_name: str, data_type: str):
        self.identifier = identifier
        self.display_name = display_name
        self.data_type = data_type

    def build(self, node, node_sockets):
        return [type_infos.build(
            self.data_type,
            node_sockets,
            self.display_name,
            self.identifier)]

    def amount(self, node):
        return 1

class ListSocketDecl(SocketDeclBase):
    def __init__(self, identifier: str, display_name: str, prop_name: str, list_or_base: str):
        self.identifier = identifier
        self.display_name = display_name
        self.prop_name = prop_name
        self.list_or_base = list_or_base

    def build(self, node, node_sockets):
        data_type = self.get_data_type(node)
        return [type_infos.build(
            data_type,
            node_sockets,
            self.display_name,
            self.identifier)]

    def get_data_type(self, node):
        base_type = getattr(node, self.prop_name)
        if self.list_or_base == "BASE":
            return base_type
        elif self.list_or_base == "LIST":
            return type_infos.to_list(base_type)
        else:
            assert False

    def amount(self, node):
        return 1

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")

class PackListDecl(SocketDeclBase):
    def __init__(self, identifier: str, prop_name: str, base_type: str):
        self.identifier_suffix = identifier
        self.prop_name = prop_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)

    def build(self, node, node_sockets):
        return list(self._build(node, node_sockets))

    def _build(self, node, node_sockets):
        for item in self.get_collection(node):
            data_type = self.base_type if item.state == "BASE" else self.list_type
            yield type_infos.build(
                data_type,
                node_sockets,
                "",
                item.identifier_prefix + self.identifier_suffix)
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def draw_socket(self, layout, node, socket, index):
        if isinstance(socket, OperatorSocket):
            props = layout.operator("fn.new_pack_list_input", text="New", emboss=False)
            props.tree_name = node.tree.name
            props.node_name = node.name
            props.prop_name = self.prop_name
        else:
            socket.draw_self(layout, node)

    def operator_socket_call(self, node, own_socket, other_socket):
        if not isinstance(other_socket, DataSocket):
            return

        is_output = own_socket.is_output
        data_type = other_socket.data_type

        if type_infos.is_base(data_type):
            if data_type != self.base_type:
                return
            state = "BASE"
        elif type_infos.is_list(data_type):
            if data_type != self.list_type:
                return
            state = "LIST"
        else:
            return

        collection = self.get_collection(node)
        item = collection.add()
        item.state = state
        item.identifier_prefix = str(uuid.uuid4())

        node.rebuild_and_try_keep_state()

        identifier = item.identifier_prefix + self.identifier_suffix
        new_socket = node.find_socket(identifier, is_output)
        node.tree.new_link(other_socket, new_socket)

    def amount(self, node):
        return len(self.get_collection(node)) + 1

    def get_collection(self, node):
        return getattr(node, self.prop_name)

    @classmethod
    def Property(cls):
        return CollectionProperty(type=PackListPropertyGroup)

class PackListPropertyGroup(bpy.types.PropertyGroup):
    bl_idname = "fn_PackListPropertyGroup"

    state: EnumProperty(
        default="BASE",
        items=[
            ("BASE", "Base", "", "NONE", 0),
            ("LIST", "Base", "", "NONE", 1)])
    identifier_prefix: StringProperty()

class NewPackListInputOperator(bpy.types.Operator):
    bl_idname = "fn.new_pack_list_input"
    bl_label = "New Pack List Input"
    bl_options = {'INTERNAL'}

    tree_name: StringProperty()
    node_name: StringProperty()
    prop_name: StringProperty()

    def execute(self, context):
        tree = bpy.data.node_groups[self.tree_name]
        node = tree.nodes[self.node_name]
        collection = getattr(node, self.prop_name)

        item = collection.add()
        item.state = "BASE"
        item.identifier_prefix = str(uuid.uuid4())

        node.rebuild_and_try_keep_state()

        return {'FINISHED'}

class AnyVariadicDecl(SocketDeclBase):
    def __init__(self, identifier: str, prop_name: str, message: str):
        self.identifier_suffix = identifier
        self.prop_name = prop_name
        self.message = message

    def build(self, node, node_sockets):
        return list(self._build(node, node_sockets))

    def _build(self, node, node_sockets):
        for item in self.get_collection(node):
            yield type_infos.build(
                item.data_type,
                node_sockets,
                item.name,
                item.identifier_prefix + self.identifier_suffix)
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def amount(self, node):
        return len(self.get_collection(node)) + 1

    def draw_socket(self, layout, node, socket, index):
        if isinstance(socket, OperatorSocket):
            props = layout.operator("fn.append_any_variadic", text=self.message, emboss=False)
            props.tree_name = node.tree.name
            props.node_name = node.name
            props.prop_name = self.prop_name
        else:
            row = layout.row(align=True)
            row.prop(self.get_collection(node)[index], "display_name", text="")
            props = row.operator("fn.remove_any_variadic", text="", icon='X')
            props.tree_name = node.tree.name
            props.node_name = node.name
            props.prop_name = self.prop_name
            props.index = index

    def get_collection(self, node):
        return getattr(node, self.prop_name)

    def operator_socket_call(self, node, own_socket, other_socket):
        if not isinstance(other_socket, DataSocket):
            return

        is_output = own_socket.is_output
        data_type = other_socket.data_type

        collection = self.get_collection(node)
        item = collection.add()
        item.data_type = data_type
        item.display_name = other_socket.name
        item.identifier_prefix = str(uuid.uuid4())

        node.rebuild_and_try_keep_state()

        identifier = item.identifier_prefix + self.identifier_suffix
        new_socket = node.find_socket(identifier, is_output)
        node.tree.new_link(other_socket, new_socket)

    @classmethod
    def Property(cls):
        return CollectionProperty(type=DataTypeGroup)

class DataTypeGroup(bpy.types.PropertyGroup):
    bl_idname = "fn_DataTypeGroup"

    data_type: StringProperty()
    display_name: StringProperty()
    identifier_prefix: StringProperty()

class AppendAnyVariadicOperator(bpy.types.Operator):
    bl_idname = "fn.append_any_variadic"
    bl_label = "Append Any Variadic"
    bl_options = {'INTERNAL'}
    bl_property = "item"

    tree_name: StringProperty()
    node_name: StringProperty()
    prop_name: StringProperty()

    def get_data_type_items(self, context):
        return type_infos.get_data_type_items()

    item: EnumProperty(items=get_data_type_items)

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        tree = bpy.data.node_groups[self.tree_name]
        node = tree.nodes[self.node_name]
        collection = getattr(node, self.prop_name)

        data_type = self.item

        item = collection.add()
        item.data_type = data_type
        item.display_name = data_type
        item.identifier_prefix = str(uuid.uuid4())

        node.rebuild_and_try_keep_state()
        return {'FINISHED'}

class RemoveAnyVariadicOperator(bpy.types.Operator):
    bl_idname = "fn.remove_any_variadic"
    bl_label = "Remove Any Variadic"
    bl_options = {'INTERNAL'}

    tree_name: StringProperty()
    node_name: StringProperty()
    prop_name: StringProperty()
    index: IntProperty()

    def execute(self, context):
        tree = bpy.data.node_groups[self.tree_name]
        node = tree.nodes[self.node_name]
        collection = getattr(node, self.prop_name)
        collection.remove(self.index)
        node.rebuild_and_try_keep_state()
        return {'FINISHED'}