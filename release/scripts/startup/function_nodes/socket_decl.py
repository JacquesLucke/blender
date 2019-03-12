from bpy.props import *
from dataclasses import dataclass
from . sockets import info

class SocketDeclBase:
    def generate(self, node, node_sockets):
        raise NotImplementedError()

class FixedSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, data_type: str):
        self.display_name = display_name
        self.data_type = data_type

    def generate(self, node, node_sockets):
        idname = info.to_idname(self.data_type)
        return node_sockets.new(idname, self.display_name)

class ListSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, type_property: str):
        self.display_name = display_name
        self.type_property = type_property

    def generate(self, node, node_sockets):
        base_type = getattr(node, self.type_property)
        idname = info.to_list_idname(base_type)
        return node_sockets.new(idname, self.display_name)

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")

class BaseSocketDecl(SocketDeclBase):
    def __init__(self, display_name: str, type_property: str):
        self.display_name = display_name
        self.type_property = type_property

    def generate(self, node, node_sockets):
        data_type = getattr(node, self.type_property)
        idname = info.to_idname(data_type)
        return node_sockets.new(idname, self.display_name)

    @classmethod
    def Property(cls):
        return StringProperty(default="Float")