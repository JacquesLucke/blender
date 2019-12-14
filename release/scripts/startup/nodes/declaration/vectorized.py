import bpy
from bpy.props import *
from . base import SocketDeclBase, NoDefaultValue
from .. types import type_infos
from .. utils.generic import getattr_recursive

class VectorizedDeclBase:
    def build(self, node_sockets):
        data_type, name = self.get_type_and_name()
        socket =type_infos.build(
            data_type,
            node_sockets,
            name,
            self.identifier)
        for prop_name, value in self.socket_settings.items():
            setattr(socket, prop_name, value)
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False

        socket = sockets[0]
        for prop_name, value in self.socket_settings.items():
            if getattr(socket, prop_name) != value:
                return False

        data_type, name = self.get_type_and_name()
        return self._data_socket_test(socket,
            name, data_type, self.identifier)

    def amount(self):
        return 1

    def get_type_and_name(self):
        if self.is_vectorized():
            return self.list_type, self.list_name
        else:
            return self.base_type, self.base_name


class VectorizedInputDecl(VectorizedDeclBase, SocketDeclBase):
    def __init__(self,
            node, identifier, prop_name,
            base_name, list_name,
            base_type, default, socket_settings):
        self.node = node
        self.identifier = identifier
        self.prop_name = prop_name
        self.base_name = base_name
        self.list_name = list_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)
        self.default = default
        self.socket_settings = socket_settings

    def init_default(self, node_sockets):
        if self.default is not NoDefaultValue:
            socket = node_sockets[0]
            socket.restore_state(self.default)

    def is_vectorized(self):
        stored = getattr_recursive(self.node, self.prop_name)
        if stored == "BASE":
            return False
        elif stored == "LIST":
            return True
        else:
            assert False

    @staticmethod
    def Property():
        return StringProperty(default="BASE")


class VectorizedOutputDecl(VectorizedDeclBase, SocketDeclBase):
    def __init__(self,
            node, identifier, input_prop_names,
            base_name, list_name,
            base_type, socket_settings):
        self.node = node
        self.identifier = identifier
        self.input_prop_names = input_prop_names
        self.base_name = base_name
        self.list_name = list_name
        self.base_type = base_type
        self.list_type = type_infos.to_list(base_type)
        self.socket_settings = socket_settings

    def is_vectorized(self):
        for prop_name in self.input_prop_names:
            if getattr_recursive(self.node, prop_name) == "LIST":
                return True
        return False
