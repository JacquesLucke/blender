import bpy
from bpy.props import *
from dataclasses import dataclass
from . function_tree import FunctionTree
from . sockets import OperatorSocket
from . types import type_infos
from . base import DataSocket
import uuid

class SocketDeclBase:
    def build(self, node_sockets):
        raise NotImplementedError()

    def amount(self):
        raise NotImplementedError()

    def draw_node(self, layout):
        pass

    def draw_socket(self, layout, socket, index):
        socket.draw_self(layout, self)

    def operator_socket_call(self, own_socket, other_socket):
        pass

class FixedSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str, data_type: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name
        self.data_type = data_type

    def build(self, node_sockets):
        return [type_infos.build(
            self.data_type,
            node_sockets,
            self.display_name,
            self.identifier)]

    def amount(self):
        return 1

class TreeInterfaceDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, tree: FunctionTree, in_or_out: str):
        assert tree is not None
        self.node = node
        self.identifier = identifier
        self.tree = tree
        self.in_or_out = in_or_out

    def build(self, node_sockets):
        if self.in_or_out == "IN":
            return list(self._build_inputs(node_sockets))
        elif self.in_or_out == "OUT":
            return list(self._build_outputs(node_sockets))
        else:
            assert False

    def _build_inputs(self, node_sockets):
        for data_type, name, identifier in self.tree.iter_function_inputs():
            yield type_infos.build(
                data_type,
                node_sockets,
                name,
                self.identifier + identifier)

    def _build_outputs(self, node_sockets):
        for data_type, name, identifier in self.tree.iter_function_outputs():
            yield type_infos.build(
                data_type,
                node_sockets,
                name,
                self.identifier + identifier)

    def amount(self):
        if self.in_or_out == "IN":
            return len(tuple(self.tree.iter_function_inputs()))
        elif self.in_or_out == "OUT":
            return len(tuple(self.tree.iter_function_outputs()))
        else:
            assert False

class ListSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str, prop_name: str, list_or_base: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name
        self.prop_name = prop_name
        self.list_or_base = list_or_base

    def build(self, node_sockets):
        data_type = self.get_data_type()
        return [type_infos.build(
            data_type,
            node_sockets,
            self.display_name,
            self.identifier)]

    def get_data_type(self):
        base_type = getattr(self.node, self.prop_name)
        if self.list_or_base == "BASE":
            return base_type
        elif self.list_or_base == "LIST":
            return type_infos.to_list(base_type)
        else:
            assert False

    def amount(self):
        return 1

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")

class PackListDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, prop_name: str, base_type: str):
        self.node = node
        self.identifier_suffix = identifier
        self.prop_name = prop_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)

    def build(self, node_sockets):
        return list(self._build(node_sockets))

    def _build(self, node_sockets):
        for item in self.get_collection():
            data_type = self.base_type if item.state == "BASE" else self.list_type
            yield type_infos.build(
                data_type,
                node_sockets,
                "",
                item.identifier_prefix + self.identifier_suffix)
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def draw_socket(self, layout, socket, index):
        if isinstance(socket, OperatorSocket):
            props = layout.operator("fn.new_pack_list_input", text="New", emboss=False)
            props.tree_name = self.node.tree.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
        else:
            row = layout.row(align=True)
            socket.draw_self(row, self.node)
            props = row.operator("fn.remove_pack_list_input", text="", icon='X')
            props.tree_name = self.node.tree.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
            props.index = index

    def operator_socket_call(self, own_socket, other_socket):
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

        collection = self.get_collection()
        item = collection.add()
        item.state = state
        item.identifier_prefix = str(uuid.uuid4())

        self.node.rebuild_and_try_keep_state()

        identifier = item.identifier_prefix + self.identifier_suffix
        new_socket = self.node.find_socket(identifier, is_output)
        self.node.tree.new_link(other_socket, new_socket)

    def amount(self):
        return len(self.get_collection()) + 1

    def get_collection(self):
        return getattr(self.node, self.prop_name)

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

class RemovePackListInputOperator(bpy.types.Operator):
    bl_idname = "fn.remove_pack_list_input"
    bl_label = "Remove Pack List Input"
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

class AnyVariadicDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, prop_name: str, message: str):
        self.node = node
        self.identifier_suffix = identifier
        self.prop_name = prop_name
        self.message = message

    def build(self, node_sockets):
        return list(self._build(node_sockets))

    def _build(self, node_sockets):
        for item in self.get_collection():
            yield type_infos.build(
                item.data_type,
                node_sockets,
                item.name,
                item.identifier_prefix + self.identifier_suffix)
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def amount(self):
        return len(self.get_collection()) + 1

    def draw_socket(self, layout, socket, index):
        if isinstance(socket, OperatorSocket):
            props = layout.operator("fn.append_any_variadic", text=self.message, emboss=False)
            props.tree_name = self.node.tree.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
        else:
            row = layout.row(align=True)
            row.prop(self.get_collection()[index], "display_name", text="")
            props = row.operator("fn.remove_any_variadic", text="", icon='X')
            props.tree_name = self.node.tree.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
            props.index = index

    def get_collection(self):
        return getattr(self.node, self.prop_name)

    def operator_socket_call(self, own_socket, other_socket):
        if not isinstance(other_socket, DataSocket):
            return

        is_output = own_socket.is_output

        item = self.add_item(other_socket.data_type, other_socket.name)
        self.node.rebuild_and_try_keep_state()

        identifier = item.identifier_prefix + self.identifier_suffix
        new_socket = self.node.find_socket(identifier, is_output)
        self.node.tree.new_link(other_socket, new_socket)

    def add_item(self, data_type, display_name):
        collection = self.get_collection()
        item = collection.add()
        item.data_type = data_type
        item.display_name = display_name
        item.identifier_prefix = str(uuid.uuid4())
        return item

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

    item: EnumProperty(items=type_infos.get_data_type_items_cb())

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