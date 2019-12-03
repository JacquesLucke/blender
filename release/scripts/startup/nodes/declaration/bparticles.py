import bpy
from bpy.props import *
from . base import SocketDeclBase
from .. sockets import OperatorSocket

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
        return True


class ExecuteInputDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        return [node_sockets.new("fn_ExecuteSocket", self.display_name, identifier=self.identifier)]

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
        for i in range(self.get_execute_amount()):
            socket = node_sockets.new(
                "fn_ExecuteSocket",
                self.display_name,
                identifier=self.identifier_prefix + str(i))
            socket.display_shape = 'SQUARE'
            yield socket
        socket = node_sockets.new(
            "fn_OperatorSocket",
            self.display_name,
            identifier=self.identifier_prefix + "(Operator)")
        socket.display_shape = 'SQUARE'
        yield socket

    def amount(self):
        return self.get_execute_amount() + 1

    def get_execute_amount(self):
        return getattr(self.node, self.prop_name)

    def validate(self, sockets):
        if len(sockets) != self.amount():
            return False

        for i, socket in enumerate(sockets[:-1]):
            expected_identifier = self.identifier_prefix + str(i)
            if socket.identifier != expected_identifier:
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
        if isinstance(socket, OperatorSocket):
            layout.label(text=self.display_name)
        else:
            layout.label(text=f"{self.display_name} ({index + 1})")

    def operator_socket_call(self, own_socket, linked_socket, connected_sockets):
        old_amount = self.get_execute_amount()
        new_amount = old_amount + 1
        new_identifier = self.identifier_prefix + str(old_amount)
        setattr(self.node, self.prop_name, new_amount)

        self.node.rebuild()

        new_socket = self.node.find_socket(new_identifier, False)
        self.node.tree.new_link(linked_socket, new_socket)

    @classmethod
    def Property(cls):
        return IntProperty(default=0)
