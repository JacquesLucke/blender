from . base import SocketDeclBase
from .. types import type_infos

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

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        return sockets[0].data_type == self.data_type

    def amount(self):
        return 1