from . base import SocketDeclBase, NoDefaultValue
from .. types import type_infos

class FixedSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str, data_type: str, default, socket_settings: dict):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name
        self.data_type = data_type
        self.default = default
        self.socket_settings = socket_settings

    def build(self, node_sockets):
        socket = type_infos.build(self.data_type,
                                  node_sockets,
                                  self.display_name,
                                  self.identifier)
        for name, value in self.socket_settings.items():
            setattr(socket, name, value)
        return [socket]

    def init_default(self, node_sockets):
        if self.default is not NoDefaultValue:
            socket = node_sockets[0]
            socket.restore_state(self.default)

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        for name, value in self.socket_settings.items():
            if getattr(socket, name) != value:
                return False
        return self._data_socket_test(sockets[0],
            self.display_name, self.data_type, self.identifier)

    def amount(self):
        return 1
