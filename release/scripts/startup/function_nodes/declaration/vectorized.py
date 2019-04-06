import bpy
from bpy.props import *
from . base import SocketDeclBase
from .. types import type_infos

class VectorizedDeclBase:
    def build(self, node_sockets):
        data_type, name = self.get_type_and_name()
        return [type_infos.build(
            data_type,
            node_sockets,
            name,
            self.identifier)]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        data_type, name = self.get_type_and_name()
        return self._data_socket_test(sockets[0],
            name, data_type, self.identifier)

    def amount(self):
        return 1

    @staticmethod
    def Property():
        return StringProperty(default="BASE")

    def get_type_and_name(self):
        stored = getattr(self.node, self.prop_name)
        if stored == "BASE":
            return self.base_type, self.base_name
        elif stored == "LIST":
            return self.list_type, self.list_name
        else:
            assert False


class VectorizedInputDecl(VectorizedDeclBase, SocketDeclBase):
    def __init__(self,
            node, identifier, prop_name,
            base_name, list_name,
            base_type):
        self.node = node
        self.identifier = identifier
        self.prop_name = prop_name
        self.base_name = base_name
        self.list_name = list_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)


class VectorizedOutputDecl(VectorizedDeclBase, SocketDeclBase):
    def __init__(self,
            node, identifier, prop_name, input_prop_names,
            base_name, list_name,
            base_type):
        assert prop_name not in input_prop_names
        self.node = node
        self.identifier = identifier
        self.prop_name = prop_name
        self.input_prop_names = input_prop_names
        self.base_name = base_name
        self.list_name = list_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)
