import bpy
import uuid
from bpy.props import *
from . base import SocketDeclBase
from .. types import type_infos
from .. base import DataSocket
from .. sockets import OperatorSocket
from .. utils.enum_items_cache import cache_enum_items

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
                item.display_name,
                item.identifier_prefix + self.identifier_suffix)
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def amount(self):
        return len(self.get_collection()) + 1

    def validate(self, sockets):
        if len(sockets) != self.amount():
            return False

        for item, socket in zip(self.get_collection(), sockets[:-1]):
            identifier = item.identifier_prefix + self.identifier_suffix
            if not self._data_socket_test(socket, item.display_name, item.data_type, identifier):
                return False

        if not isinstance(sockets[-1], OperatorSocket):
            return False

        return True

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

    def operator_socket_call(self, own_socket, linked_socket, connected_sockets):
        connected_types = {s.data_type for s in connected_sockets if isinstance(s, DataSocket)}
        if len(connected_types) != 1:
            return

        connected_socket = next(iter(connected_sockets))
        connected_type = next(iter(connected_types))
        connected_node = connected_socket.node

        is_output = own_socket.is_output

        item = self.add_item(connected_type, connected_socket.name)
        self.node.rebuild()

        identifier = item.identifier_prefix + self.identifier_suffix
        new_socket = self.node.find_socket(identifier, is_output)
        self.node.tree.new_link(linked_socket, new_socket)

    def add_item(self, data_type, display_name):
        collection = self.get_collection()
        item = collection.add()
        item.data_type = data_type
        item.display_name = display_name
        item.identifier_prefix = str(uuid.uuid4())
        self.node.refresh()
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

    item: EnumProperty(items=cache_enum_items(type_infos.get_data_type_items_cb()))

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

        node.refresh()
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
        node.refresh()
        return {'FINISHED'}
