from bpy.props import *
from dataclasses import dataclass
from . sockets import type_infos

class SocketDeclBase:
    def build(self, node, node_sockets):
        raise NotImplementedError()

class FixedSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, data_type: str):
        self.display_name = display_name
        self.data_type = data_type

    def build(self, node, node_sockets):
        return type_infos.build(
            self.data_type,
            node_sockets,
            self.display_name)

class ListSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, type_property: str):
        self.display_name = display_name
        self.type_property = type_property

    def build(self, node, node_sockets):
        base_type = getattr(node, self.type_property)
        list_type = type_infos.to_list(base_type)
        return type_infos.build(
            list_type,
            node_sockets,
            self.display_name)

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")

class BaseSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, type_property: str):
        self.display_name = display_name
        self.type_property = type_property

    def build(self, node, node_sockets):
        data_type = getattr(node, self.type_property)
        return type_infos.build(
            data_type,
            node_sockets,
            self.display_name)

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")