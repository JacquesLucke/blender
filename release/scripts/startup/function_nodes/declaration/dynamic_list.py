from bpy.props import *
from . base import SocketDeclBase
from .. types import type_infos

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

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        return self._data_socket_test(sockets[0],
            self.display_name, self.get_data_type(), self.identifier)

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
