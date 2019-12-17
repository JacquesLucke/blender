import bpy
import uuid
from bpy.props import *
from . base import SocketDeclBase
from .. sockets import OperatorSocket, ExecuteSocket

MAX_LINK_LIMIT = 4095

class InfluencesSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("fn_InfluencesSocket", self.display_name, identifier=self.identifier)
        socket.link_limit = MAX_LINK_LIMIT
        socket.display_shape = 'DIAMOND'
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "fn_InfluencesSocket":
            return False
        if socket.name != self.display_name:
            return False
        if socket.link_limit != MAX_LINK_LIMIT:
            return False
        return True

    def amount(self):
        return 1

class ExecuteOutputDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("fn_ExecuteSocket", self.display_name, identifier=self.identifier)
        socket.display_shape = 'SQUARE'
        return [socket]

    def amount(self):
        return 1

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.name != self.display_name:
            return False
        elif socket.identifier != self.identifier:
            return False
        elif socket.bl_idname != "fn_ExecuteSocket":
            return False
        return True


class ExecuteInputDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("fn_ExecuteSocket", self.display_name, identifier=self.identifier)
        socket.display_shape = "SQUARE"
        return [socket]

    def amount(self):
        return 1

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        if sockets[0].bl_idname != "fn_ExecuteSocket":
            return False
        return True


class ExecuteInputListDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, prop_name: str, display_name: str):
        self.node = node
        self.identifier_prefix = identifier
        self.display_name = display_name
        self.prop_name = prop_name

    def build(self, node_sockets):
        return list(self._build(node_sockets))

    def _build(self, node_sockets):
        items = self.get_items()
        for i, item in enumerate(items):
            socket = node_sockets.new(
                "fn_ExecuteSocket",
                self.display_name if i == 0 else "Then",
                identifier=item.identifier)
            socket.display_shape = 'SQUARE'
            yield socket
        socket = node_sockets.new(
            "fn_OperatorSocket",
            self.display_name,
            identifier=self.identifier_prefix + "(Operator)")
        socket.display_shape = 'SQUARE'
        yield socket

    def amount(self):
        return len(self.get_items()) + 1

    def get_items(self):
        return getattr(self.node, self.prop_name)

    def validate(self, sockets):
        if len(sockets) != self.amount():
            return False

        for socket, item in zip(sockets[:-1], self.get_items()):
            if socket.identifier != item.identifier:
                return False
            elif socket.bl_idname != "fn_ExecuteSocket":
                return False

        if not isinstance(sockets[-1], OperatorSocket):
            return False
        if not sockets[-1].name == self.display_name:
            return False
        if not sockets[-1].identifier == self.identifier_prefix + "(Operator)":
            return False

        return True

    def draw_socket(self, layout, socket, index):
        row = layout.row(align=True)
        if index == 0:
            row.label(text=self.display_name)
        else:
            row.label(text="Then")
        if isinstance(socket, ExecuteSocket):
            props = row.operator("fn.remove_execute_socket", text="", icon="X")
            props.tree_name = socket.id_data.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
            props.index = index

    def operator_socket_call(self, own_socket, linked_socket, connected_sockets):
        item = self.get_items().add()
        item.identifier = str(uuid.uuid4())

        self.node.rebuild()

        new_socket = self.node.find_socket(item.identifier, False)
        self.node.tree.new_link(linked_socket, new_socket)

    @classmethod
    def Property(cls):
        return CollectionProperty(type=ExecuteInputItem)


class ExecuteInputItem(bpy.types.PropertyGroup):
    identifier: StringProperty()


class RemoveExecuteSocketOperator(bpy.types.Operator):
    bl_idname = "fn.remove_execute_socket"
    bl_label = "Remove Execute Socket"
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
        tree.sync()
        return {'FINISHED'}
